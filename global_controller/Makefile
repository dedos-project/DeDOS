TARGETS = global_controller
CC:=$(CROSS_COMPILE)gcc
ADDRESS_SANITIZER?=0
DEBUG=1

CFLAGS=-I$(PREFIX)/include -Iinclude -Wall -Wdeclaration-after-statement -W -Wextra -Wshadow -Wcast-qual -Wwrite-strings -Wunused-variable -Wundef -Wunused-function
CFLAGS+= -Wconversion
CFLAGS+= -Wcast-align
CFLAGS+= -Wmissing-prototypes
CFLAGS+= -Wno-missing-field-initializers

ifeq ($(DEBUG),1)
  CFLAGS+=-ggdb
endif
ifeq ($(ADDRESS_SANITIZER),1)
  TEST_LDFLAGS+=-fsanitize=address -fno-omit-frame-pointer
endif

CORE_OBJ= src/main.o \
		  src/dfg.o \
		  src/api.o \
		  src/jsmn.o \
		  src/scheduling.o \
		  src/communication.o \
		  src/cli_interface.o \
		  src/control_msg_handler.o\
		  src/stat_msg_handler.o

all: $(TARGETS)

global_controller: $(CORE_OBJ)
	$(CC) -pthread -o $@ $^ -lm $(CFLAGS) $(TEST_LDFLAGS)

clean:
	rm -f $(TARGETS) $(CORE_OBJ)
