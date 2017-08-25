#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <xcb/xcb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MASTER_SIZE 0.6
#define BORDER 3
#define GAP 1
#define PIPE_FILE "/tmp/hoot"
#define MIN 10 // minimum window size
#define DESKTOPS 3

#define XCB_MOVE XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
#define XCB_RESIZE XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
#define XCB_MOVE_RESIZE XCB_MOVE | XCB_RESIZE
#define move_resize(win, x, y, w, h) \
    uint32_t tmpvals[4] = { x, y, w, h }; \
    xcb_configure_window(conn, win, XCB_MOVE_RESIZE, tmpvals);
#define p(s) printf("hootwm: %s\n", s)

typedef struct node node;
typedef struct desktop desktop;

struct node {
    xcb_window_t win;
    struct node *next;
    struct node *prev;
};

struct desktop {
    node *head;
    node *current;
};

xcb_connection_t *conn;
xcb_screen_t     *screen;
int16_t           pipe_fd;
char             *pipe_f;

uint16_t screen_w, screen_h,
         master_size;

uint8_t gap, bord;

uint32_t win_focus, win_unfocus;

node *head, *current;
desktop *current_desktop,
        *desktops[DESKTOPS];

bool run;

// Prototypes are shit
void quit() {
    run = false;
    p("Thanks for using!");

    //for (node *w = head; w; w = w->next) free(w);
    for (int8_t i = 0; i < DESKTOPS; free(desktops[i++]));

    close(pipe_fd); unlink(pipe_f);
    xcb_disconnect(conn);

    exit(0);
}

// Manage window stack

void node_insert_at_head(node *n) {
    if (head) {
        head->prev = n;
        n->next = head;
    }
    head = n;
}

void node_insert_at_tail(node *n) {
    if (head) {
        node *tmp = head;
        while (tmp->next) tmp = tmp->next;
        tmp->next = n;
        n->prev = tmp;
    } else {
        head = n;
    }
}

void node_remove(node *n) {
    if (n == head) head = n->next;
    if (n->next) n->next->prev = n->prev;
    if (n->prev) n->prev->next = n->next;
}

void node_swap(node *a, node *b) {
    xcb_window_t tmp;
    tmp = a->win;
    a->win = b->win;
    b->win = tmp;
}

void win_create(xcb_window_t win) {
    node *w = (node*)calloc(1, sizeof(node));
    w->win = win;
    w->next = NULL; w->prev = NULL;

    node_insert_at_tail(w);
    current = w;
}

void win_destroy(xcb_window_t win) {
    node *w;
    int8_t i;
    for (i = DESKTOPS; i; --i) {
        w = desktops[i-1]->head;
        while (w) {
            if (w->win == win) goto rest;
            w = w->next;
        }
    }

    if (!w) return;
rest:
    node_remove(w);

    if (w == current) {
        if (w->next) current = w->next;
        else current = w->prev;
    }

    if (w == desktops[i-1]->head) desktops[i-1]->head = NULL;
    if (w == desktops[i-1]->current) desktops[i-1]->current = NULL;

    free(w);
}

void win_swap(node *a, node *b) {
    if (a == current) current = b;
    else if (b == current) current = a;

    node_swap(a, b);
}

void win_move_down(node *w) {
    if (w->next) {
        if (w == current) current = w->next;
        node_swap(w, w->next);
    }
}

void win_move_up(node *w) {
    if (w != head) {
        if (w == current) current = w->prev;
        node_swap(w, w->prev);
    }
}

void desktop_switch(uint8_t i) {
    if (i > DESKTOPS || current_desktop == desktops[i-1]) return;
    for (node *w = head; w; w = w->next) {
        xcb_unmap_window(conn, w->win);
    }

    current_desktop->head = head;
    current_desktop->current = current;

    current_desktop = desktops[i-1];
    head = current_desktop->head;
    current = current_desktop->current;

    for (node *w = head; w; w = w->next) {
        xcb_map_window(conn, w->win);
    }
}

// Move windows
void tile(void) {
    if (!head) {
        return;
    } else if (!head->next) {
        move_resize(head->win, gap, gap,
            screen_w - gap*2 - bord*2,
            screen_h - gap*2 - bord*2);
    } else {
        move_resize(head->win, gap, gap,
            master_size - gap*2 - bord*2,
            screen_h - gap*2 - bord*2);

        node *w = head;
        uint16_t y = 0; uint8_t n = 0;
        while (w->next) { ++n; w = w->next; };
        uint16_t height = screen_h / n;
        for (w = head->next ; n > 0 ;
                --n, w = w->next, y += height) {
            move_resize(w->win,
                master_size + gap, y + gap,
                screen_w - master_size - gap*2 - bord*2,
                ((w->next) ? height : (screen_h - y)) -
                gap*2 - bord*2);
        }
    }
}

void update_current(void) {
    node *w;

    for (w = head; w; w = w->next) {
        uint32_t b[1] = { bord };
        xcb_configure_window(conn, w->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, b);

        if (w == current) {
            xcb_change_window_attributes(conn, w->win, XCB_CW_BORDER_PIXEL,
                &win_focus);
            xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                w->win, XCB_CURRENT_TIME);
        } else {
            xcb_change_window_attributes(conn, w->win, XCB_CW_BORDER_PIXEL,
                &win_unfocus);
        }
    }
}

// Handle events
void map_request(xcb_map_request_event_t *ev) {
    win_create(ev->window);

    tile();
    xcb_map_window(conn, ev->window);
    update_current();
}

void destroy_notify(xcb_destroy_notify_event_t *ev) {
    win_destroy(ev->window);

    tile();
    update_current();
}

// Manage commands
void dispatch_command(char command[]) {
    char comm_list[2][16];
    uint16_t i = 0,
             w = 0, // word
             wi = 0; // word index
    
    do {
        if (command[i] != ' ') {
            comm_list[w][wi++] = command[i];
        } else {
            comm_list[w++][wi] = '\0';
            wi = 0;
        }
    } while (command[i++] != '\0' && w < 2);

    if (w == 0) {
        if (!strcmp("quit", comm_list[0])) {
            quit();
        }
    }

    if (w < 1) return;

    if (!strcmp("grow", comm_list[0])) {
        //int32_t s = strtonum(comm_list[1], -screen_w, screen_w, NULL);
        int32_t s = strtoll(comm_list[1], NULL, 10);
        if (!s) {
            return;
        } else if (s > 0) {
            if (s + master_size > screen_w - (MIN + bord*2 + gap*2)) {
                master_size  = screen_w - (MIN + bord*2 + gap*2);
            } else master_size += s;
        } else {
            if (s + master_size < (MIN + bord*2 + gap*2)) {
                master_size = MIN + bord*2 + gap*2;
            } else master_size += s;
        }

        tile();
        update_current();
    } else if (!strcmp("switch", comm_list[0])) {
        //uint8_t s = strtonum(comm_list[1], 0, DESKTOPS, NULL);
        uint8_t s = strtoll(comm_list[1], NULL, 10);
        if (!s) return;

        desktop_switch(s);
        tile();
        update_current();
    } else if (!current) {
        return;
    // Here be functions that require a window
    } else if (!strcmp("move", comm_list[0])) {
        if (!strcmp("up", comm_list[1])) {
            win_move_up(current);
            tile();
            update_current();
        } else if (!strcmp("down", comm_list[1])) {
            win_move_down(current);
            tile();
            update_current();
        }
    } else if (!strcmp("focus", comm_list[0])) {
        if (!strcmp("up", comm_list[1])) {
            if (current->prev) current = current->prev;
            update_current();
        } else if (!strcmp("down", comm_list[1])) {
            if (current->next) current = current->next;
            update_current();
        }
    }
}
// Run
void setup(void) {
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    pipe_f = PIPE_FILE;
    mkfifo(pipe_f, 0666);
    pipe_fd = open(pipe_f, O_RDONLY | O_NONBLOCK);

    screen_w = screen->width_in_pixels;
    screen_h = screen->height_in_pixels;

    head = NULL;
    current = NULL;

    master_size = screen_w * MASTER_SIZE;
    gap = GAP;
    bord = BORDER;

    win_focus = 52260;
    win_unfocus = 34184;

    run = true;

    for (int8_t i = 0; i < DESKTOPS; ++i) {
        desktops[i] = (desktop*)calloc(1,sizeof(desktop));
        desktops[i]->head = NULL;
        desktops[i]->current = NULL;
    }
    current_desktop = desktops[0];

    uint32_t mask[1] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | // destroy notify
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT }; // map request
    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, mask);

    xcb_flush(conn);
}

void event_loop(void) {
    uint32_t length;
    xcb_generic_event_t *ev;

    do {
        if ((ev = xcb_poll_for_event(conn))) {
            switch(ev->response_type & ~0x80) {
            case XCB_MAP_REQUEST: {
                map_request((xcb_map_request_event_t*)ev);
                xcb_flush(conn);
                } break; 
            case XCB_DESTROY_NOTIFY: {
                destroy_notify((xcb_destroy_notify_event_t*)ev);
                xcb_flush(conn);
                } break; 
            }
        }

        char buffer[255];

        if ((length = read(pipe_fd, buffer, sizeof(buffer)))) {
            buffer[length-1] = '\0';
            dispatch_command(buffer);
            xcb_flush(conn);
        }

        free(ev);

        struct timespec t = { 0, 30000000L };
        nanosleep(&t, NULL);

    } while (run);
}

int main(void) {
    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) return 1;

    setup();
    signal(SIGINT, quit);
    p("Welcome to hootwm.");
    event_loop();

    return 0;
}
