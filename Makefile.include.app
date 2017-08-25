
ifeq ($CROSS_COMPILE, )
ifeq ($ARCH, arm)
CROSS_COMPILE = arm-none-linux-gnueabi-
endif
endif

ifeq ($ARCH, pc)
CROSS_COMPILE = 
endif 

STRIP = $(CROSS_COMPILE)strip
CC = $(CROSS_COMPILE)gcc 
CFLAGS += -Werror -Wfatal-errors -Wunused

OBJS+=$(filter %.o,$(SRC:.c=.o))

all: $(TARGET)

%.o: %.c
	@rm -f $@
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(STRIP) $(TARGET)

clean:
	@for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f $(TARGET)
	@rm -f .stamp_built .stamp_target_installed .stamp_staging_installed

