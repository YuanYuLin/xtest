#include <xcb/xcb.h>

int
main ()
{
  xcb_connection_t *c;
  xcb_screen_t     *screen;
  xcb_drawable_t    win;
  xcb_gcontext_t    black;
  uint32_t          mask;
  uint32_t          value[1];

  /* Open the connection to the X server and get the first screen */
  c = xcb_connect (NULL, NULL);
  screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;

  /* Create a black graphic context for drawing in the foreground */
  win = screen->root;
  black = xcb_generate_id (c);
  mask = XCB_GC_FOREGROUND;
  value[0] = screen->black_pixel;
  xcb_create_gc (c, black, win, mask, value);

  return 0;
}
