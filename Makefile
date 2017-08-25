
TARGET = mini_man

CFLAGS += -I$(SDKSTAGE)/usr/include/xcb
CFLAGS += -I$(SDKSTAGE)/usr/include/
CFLAGS += -I./include/

LDFLAGS += -L$(SDKSTAGE)/usr/lib/ 
LDFLAGS += -lc -lpthread -lrt
LDFLAGS += -lxcb -lXau -lXdmcp

SRC = 
SRC += mini_man.c
#SRC += sysinit.c

#SRC += configs/platform.c

ifeq ($(SUPPORT_PLATFORM_AS_HOST),y)
CFLAGS += -DSUPPORT_PLATFORM_AS_HOST
endif
ifeq ($(SUPPORT_PLATFORM_AS_GUEST),y)
endif


include Makefile.include.app
