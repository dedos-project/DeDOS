TARGETS = global_controller
CC:=$(CROSS_COMPILE)gcc
ADDRESS_SANITIZER?=0
DEBUG=1

LOGS = \
	   DEBUG \
	   INFO \
	   ERROR \
	   WARN \
	   CRITICAL \
	   CUSTOM \

LOG_DEFINES=$(foreach logname, $(LOGS), -DLOG_$(logname)=1)

SELF=./Makefile

COMMON = ../common/
COMMON_SRC = $(COMMON)src/
COMMON_BLD = $(COMMON)build/
COMMON_OBJ = $(COMMON_BLD)objs/


CFLAGS= -std=gnu99 -I$(PREFIX)/include -I$(COMMON_SRC) -Iinclude -Wall
CFLAGS+= -Wconversion
CFLAGS+= -Wcast-align
CFLAGS+= -Wmissing-prototypes
CFLAGS+= -Wno-missing-field-initializers
CFLAGS+= $(LOG_DEFINES)

ifeq ($(DEBUG),1)
  CFLAGS+=-ggdb
endif
ifeq ($(ADDRESS_SANITIZER),1)
  TEST_LDFLAGS+=-fsanitize=address -fno-omit-frame-pointer
endif

CORE_OBJ= src/main.o \
		  src/api.o \
		  src/scheduling.o \
		  src/communication.o \
		  src/cli_interface.o \
		  src/control_msg_handler.o\
		  src/stat_msg_handler.o
COMMON_SRCS = $(wildcard $(COMMON_SRC)*.c)
COMMON_OBJS = $(patsubst $(COMMON_SRC)%.c, $(COMMON_OBJ)%.o, $(COMMON_SRCS))

all: $(TARGETS)

global_controller:: $(SELF)

$(COMMON_OBJ)%.o: $(COMMON_SRC)%.c
	cd .. && make common

global_controller:: $(CORE_OBJ) $(COMMON_OBJS)
	$(CC) -pthread -o $@ $^  -lm $(CFLAGS) $(TEST_LDFLAGS)

clean:
	rm -f $(TARGETS) $(CORE_OBJ)
