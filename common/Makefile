CC=gcc -c
LD=ld -r
DEPENDS=gcc -MM -MF
FINAL=gcc

PRINT_COMPILATION=0

LOGS = \
	   	INFO \
	   	ERROR \
	  	WARN \
	   	CRITICAL \
	   	CUSTOM \

LOG_DEFINES=$(foreach logname, $(LOGS), -DLOG_$(logname)=1)

SELF=./Makefile

CFLAGS=-Wall -g --std=gnu99 $(LOG_DEFINES)

SRC=src/
BLD=build/
OBJ=build/objs/
DEP=build/deps/
RES=build/results/
PATH_TST=test/

DIRS=$(BLD) $(OBJ) $(DEP) $(RES)

SRC_TST = $(wildcard $(PATH_TST)*.c)
LFLAGS= -lpthread -lm
TEST_CFLAGS= $(CFLAGS) -I$(SRC) -lcheck_pic -lrt -lc -lpcap $(LFLAGS)
RESULTS = $(patsubst $(PATH_TST)Test_%.c, $(RES)Test_%.txt, $(SRC_TST))
DEP_TST = $(patsubst $(PATH_TST)Test_%.c, $(PATH_DEP)Test_%.d, $(SRC_TST))

SRCS=$(wildcard $(SRC)*.c)
HDRS=$(wildcard $(SRC)*.h)
OBJS=$(patsubst $(SRC)%.c, $(OBJ)%.o, $(SRCS))
DEPS=$(patsubst $(SRC)%.c, $(DEP)%.d, $(SRCS))

all: dirs $(OBJS)

dirs: $(DIRS)

$(DIRS):
	mkdir -p $@

ifeq ($(PRINT_COMPILATION), 1)
$(OBJ)%.o: $(SRC)%.c $(SELF)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(SRC)
else
$(OBJ)%.o: $(SRC)%.c $(SELF)
	@echo "*** COMPILING: $@ "
	@$(CC) -c $< -o $@ $(CFLAGS) -I$(SRC)
endif


$(DEP)%.d: $(SRC)%.c $(DEP)
	@$(DEPENDS) $@ -MT $(patsubst $(DEP)%.d, $(OBJ)%.o, $@) $(CFLAGS) $<

clean:
	rm -rf $(DIRS)

.PHONY: clean global_controller runtime common dirs
FORCE:;

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif


test: $(RESULTS)
	@echo "-----------------------\nTEST OUTPUT:\n-----------------------"
	@for FILE in $^; do \
		if [ "$$FILE" != test_resources ]; then \
   			echo ___ $$FILE ___ ; \
			cat $$FILE; \
		fi;\
	done
	@echo "-----------------------\nFAILURES:\n-----------------------"
	@-grep -s "F:" $^; echo "";
	@echo "-----------------------\nERRORS:\n-----------------------"
	@-grep -s "E:" $^; echo "";
	@echo "\nDONE"

# Output the results of the tests by executing each of the builds
# of the tests. Output STDOUT and STDERR to the name of the rule
$(RES)%.txt: $(BLD)%.out $(TEST_RESOURCES)
	-./$< > $@ 2>&1


# creates the test executables by linking the test objects with
# the build objects, unity, and the test dependencies
$(BLD)Test_%.out: $(OBJ)Test_%.o $(OBJS)
	$(FINAL) -o $@ $(filter-out $(patsubst $(BLD)Test_%.out, $(OBJ)%.o, $@), $^) $(TEST_CFLAGS)

# Creates object files from the test sources
$(OBJ)%.o:: $(PATH_TST)%.c $(SELF)
	$(CC) $(TEST_CFLAGS) $< -o $@

.PHONY: test
.PRECIOUS: $(BLD)Test_%.out
