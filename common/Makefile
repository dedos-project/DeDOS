CC=gcc -c
LD=ld -r
DEPENDS=gcc -MM -MF


LOGS = \
		DEBUG\
	   	INFO \
	   	ERROR \
	  	WARN \
	   	CRITICAL \
	   	CUSTOM

LOG_DEFINES=$(foreach logname, $(LOGS), -DLOG_$(logname)=1)

SELF=./Makefile

CFLAGS=-Wall -g --std=gnu99 $(LOG_DEFINES)

SRC=src/
BLD=build/
OBJ=build/objs/
DEP=build/deps/

DIRS=$(BLD) $(OBJ) $(DEP)

SRCS=$(wildcard $(SRC)*.c)
HDRS=$(wildcard $(SRC)*.h)
OBJS=$(patsubst $(SRC)%.c, $(OBJ)%.o, $(SRCS))
DEPS=$(patsubst $(SRC)%.c, $(DEP)%.d, $(SRCS))

all: dirs $(OBJS)

dirs: $(DIRS)

$(DIRS):
	mkdir -p $@

$(OBJ)%.o: $(SRC)%.c $(SELF)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(SRC)

$(DEP)%.d: $(SRC)%.c $(DEP)
	$(DEPENDS) $@ -MT $(patsubst $(DEP)%.d, $(OBJ)%.o, $@) $(CFLAGS) $<

clean:
	rm -rf $(DIRS)

.PHONY: clean global_controller runtime common dirs
FORCE:;

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif


