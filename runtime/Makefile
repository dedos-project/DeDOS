ADDRESS_SANITIZER?=0
DATAPLANE_PROFILING=0
DEBUG = 1
COLLECT_STATS = 1
DUMP_STATS = 1

LOGS = \
	   INFO \
	   ERROR \
	   WARN \
	   CRITICAL \

SRC_DIR = src/
RNT_DIR = $(SRC_DIR)runtime/
MSU_DIR = $(SRC_DIR)msu/
COM_DIR = $(SRC_DIR)common/
LEG_DIR = $(SRC_DIR)legacy/
TST_DIR = $(SRC_DIR)test/

SRC_DIRS = $(RNT_DIR) $(COM_DIR) $(MSU_DIR) $(wildcard $(MSU_DIR)*/)

TARGET=rt
MAIN=$(RNT_DIR)main.c

BLD_DIR = build/
DEP_DIR = $(BLD_DIR)depends/
OBJ_DIR = $(BLD_DIR)objs/
INC_DIR = $(BLD_DIR)include/
RES_DIR = $(BLD_DIR)reults/
TST_BLD_DIR = $(BLD_DIR)test/
LEG_BLD_DIR = $(BLD_DIR)legacy/

BLD_DIRS = $(BLD_DIR) $(DEP_DIR) $(OBJ_DIR) $(INC_DIR) $(RES_DIR) $(LEG_BLD_DIR)

#LEGACY_LIBS = picotcp

CLEANUP=rm -f
CLEAN_DIR=rm -rf
MKDIR=mkdir -p

CC:=gcc -c
CXX:=g++ -c

COMPILE=gcc -c
COMPILE_PP=g++ -c
LD=ld -r
FINAL=gcc
DEPEND=gcc -MM -MF

SELF=./Makefile

LOG_DEFINES=$(foreach logname, $(LOGS), -DLOG_$(logname))

CFLAGS=-Wall -pthread -lpcre -lvdeplug -lssl -lcrypto -lm -lpcap --std=gnu99  $(LOG_DEFINES)

ifeq ($(DEBUG), 1)
  CFLAGS+=-ggdb
endif

ifeq ($(DATAPLANE_PROFILING),1)
  CFLAGS+=-DDATAPLANE_PROFILING
endif

ifeq ($(ADDRESS_SANITIZER),1)
  CFLAGS+=-fsanitize=address -fno-omit-frame-pointer
endif

ifeq ($(COLLECT_STATS), 1)
  CFLAGS+=-DCOLLECT_STATS=1
endif

ifeq ($(DUMP_STATS), 1)
  CFLAGS+=-DDUMP_STATS=1
endif

LEG_MAKE=$(foreach LEG_LIB, $(LEGACY_LIBS), $(LEG_DIR)$(LEG_LIB)/Makefile)
LEG_INC=$(foreach LEG_LIB, $(LEGACY_LIBS), $(LEG_DIR)$(LEG_LIB)/build/include)
LEG_SRC=$(foreach LEG_LIB, $(LEGACY_LIBS), $(wildcard $(LEG_DIR)$(LEG_LIB)/src/*))
LEG_OBJ=$(foreach LEG_LIB, $(LEGACY_LIBS), $(LEG_BLD_DIR)$(LEG_LIB).o)


TSTS = $(wildcard $(TST_DIR)*/*.c)

SRCS = $(foreach src_dir, $(SRC_DIRS), $(wildcard $(src_dir)*.c))
HDRS = $(foreach src_dir, $(SRC_DIRS), $(wildcard $(src_dir)*.h))

RESULTS = $(patsubst $(TST_DIR)%.c, $(RES_DIR)%.txt, $(TSTS))

DEP_DIRS = $(patsubst $(SRC_DIR)%/, $(DEP_DIR)%/, $(SRC_DIRS))
DEP_TST = $(patsubst $(TST_DIR)%.c, $(DEP_DIR)%.d, $(TSTS))
DEP_SRC = $(patsubst $(SRC_DIR)%.c, $(DEP_DIR)%.d, $(SRCS))

OBJ_DIRS = $(patsubst $(SRC_DIR)%/, $(OBJ_DIR)%/, $(SRC_DIRS))
OBJECTS = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.o, $(SRCS))
OBJECTS_NOMAIN = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.o, $(filter-out $(MAIN), $(SRCS)))

TST_DIRS = $(patsubst $(SRC_DIR)%/, $(TST_BLD_DIR)%/, $(SRC_DIRS))
RES_DIRS = $(patsubst $(SRC_DIR)%/, $(RES_DIR)%/, $(SRC_DIRS))

INCS=$(LEG_INC) $(RNT_DIR) $(COM_DIR) $(MSU_DIR)

CFLAGS+= $(foreach inc, $(INCS), -I$(inc))

TEST_CFLAGS= $(CFLAGS) -lcheck_pic -lrt -lc -lpcap -lm


all: dirs includes legacy ${TARGET}

includes: $(INC_DIRS) $(INC_HDRS)

dirs: $(BUILD_DIRS) $(OBJ_DIRS) $(DEP_DIRS) $(TST_DIRS) $(RES_DIRS)

$(INC_DIR)%.h: $(SRC_DIR)%.h
	cp $^ $@

legacy: ${LEG_OBJ}

depends: $(DEP_DIRS) ${DEP_SRC}

$(LEG_BLD_DIR)%.o:: $(LEG_DIR)%
	@filename=$$(basename "$@"); filename="$${filename%.*}"; echo $$filename; cd $(LEG_DIR)/$$filename && make;
	@echo ___________ $< ___________
	$(LINK) -o $@ $</build/*.o

$(TARGET): ${OBJECTS} ${LEG_OBJ}
	$(FINAL) -o $@ $^ $(CFLAGS)

test: $(RESULTS)
	@echo "-----------------------\nTEST OUTPUT:\n-----------------------"
	@for FILE in $^; do \
		echo ___ $$FILE ___ ; \
		cat $$FILE; \
	done
	@echo "-----------------------\nFAILURES:\n-----------------------"
	@-grep -s "F:" $^; echo "";
	@echo "-----------------------\nERRORS:\n-----------------------"
	@-grep -s "E:" $^; echo "";
	@echo "\nDONE"

# Output the results of the tests by executing each of the builds
# of the tests. Output STDOUT and STDERR to the name of the rule
$(RES_DIR)%.txt: $(TST_BLD_DIR)%
	-./$< > $@ 2>&1

# creates the test executables by linking the test objects with the build objects excluding 
# the specific source under test
$(TST_BLD_DIR)%: $(TST_DIR)%.c $(OBJECTS_NOMAIN) $(LEG_OBJ)
	echo $(subst Test_,, $(patsubst $(TST_BLD_DIR)%, $(OBJ_DIR)%.o, $@))
	$(FINAL) -o $@ $(filter-out $(subst Test_,, $(patsubst $(TST_BLD_DIR)%, $(OBJ_DIR)%.o, $@)), $^) $(TEST_CFLAGS)

# Creates object files from the source file
$(OBJ_DIR)%.o:: $(SRC_DIR)%.c $(SELF)
	$(COMPILE) $(CFLAGS) $< -o $@

$(DEP_SRC): $(DEP_DIRS)

$(DEP_DIR)%.d:: $(SRC_DIR)%.c $(LEG_OBJ)
	@$(DEPEND) $@ -MT $(patsubst $(DEP_DIR)%.d, $(OBJ_DIR)%.o, $@) $(TEST_CFLAGS) $<

$(BLD_DIRS) $(DEP_DIRS) $(INC_DIRS) $(OBJ_DIRS) $(TST_DIRS) $(RES_DIRS):
	@$(MKDIR) $@

clean:
	$(CLEAN_DIR) $(BLD_DIR)
	$(foreach leg_lib, $(LEG_LIBS), $(CLEAN_DIR) $(PATH_SRC_LEG)$(leg_lib)/build)
	$(CLEANUP) $(TARGET)

ifneq ($(MAKECMDGOALS), clean)
-include $(DEP_TST)
-include $(DEP_SRC)
endif

.PHONY: all
.PHONY: clean
.PHONY: test
.PHONY: legacy
.PHONY: depends
.PRECIOUS: $(DEP_DIR)%.d $(RES_DIR)%.txt $(OBJ_DIR)%.o $(TST_BLD_DIR)%
