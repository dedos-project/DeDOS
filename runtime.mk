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
	   CONNECTIONS \
	   CUSTOM \
	   DFG_PARSING \
	   ALL

NO_LOGS = \
		  JSMN_PARSING \
		  DFG_PARSING
		  #MSU_ENQUEUES \
		  MSU_DEQUEUES

SRC_DIR = src/
RNT_DIR = $(SRC_DIR)runtime/
MSU_DIR = $(SRC_DIR)msus/
COM_DIR = $(SRC_DIR)common/
LEG_DIR = $(SRC_DIR)legacy/
TST_DIR = test/

MSU_DIRS = $(MSU_DIR) $(filter %/, $(wildcard $(MSU_DIR)*/))

SRC_DIRS = $(RNT_DIR) $(COM_DIR) $(MSU_DIRS)

TARGET=rt
MAIN=$(RNT_DIR)main.c

BLD_DIR = build/
DEP_DIR = $(BLD_DIR)depends/
OBJ_DIR = $(BLD_DIR)objs/
RES_DIR = $(BLD_DIR)reults/
TST_BLD_DIR = $(BLD_DIR)test/
LEG_BLD_DIR = $(BLD_DIR)legacy/

BLD_DIRS = $(BLD_DIR) $(DEP_DIR) $(OBJ_DIR) $(RES_DIR) $(LEG_BLD_DIR)
BLD_DIRS += $(patsubst $(SRC_DIR)%/, $(OBJ_DIR)%/, $(SRC_DIRS))

LEGACY_LIBS = picotcp

CLEANUP=rm -f
CLEAN_DIR=rm -rf
MKDIR=mkdir -p

OPTIM=6

CC:=gcc
CXX:=g++

COMPILE=$(CC) -c
COMPILE_PP=$(CXX) -c
DEPEND=$(CC) -MM -MF
DEPEND_PP=$(CXX) -MM -MF
LINK=ld -r
FINAL=$(CXX)
FINAL_TEST=$(CC)

SELF=./runtime.mk

LOG_DEFINES=$(foreach logname, $(LOGS), -DLOG_$(logname)) \
			$(foreach logname, $(NO_LOGS), -DNO_LOG_$(logname))

CFLAGS=-Wall -pthread -lpcre -lvdeplug -lssl -lrt -lcrypto -lm -lpcap -O$(OPTIM) $(LOG_DEFINES)
CC_EXTRAFLAGS = --std=gnu99

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

RESOURCE_EXTS=.txt .json

TST_DIRS = $(patsubst $(SRC_DIR)%/, $(TST_DIR)%/, $(SRC_DIRS))

TSTS = $(foreach TST_D, $(TST_DIRS), $(wildcard $(TST_D)*.c))
TST_MKS = $(foreach TST_D, $(TST_DIRS), $(wildcard $(TST_D)*.mk))
TST_RSCS = $(foreach TST_D, $(TST_DIRS), $(foreach EXT, $(RESOURCE_EXTS), $(wildcard $(TST_D)*$(EXT))))

SRCS = $(foreach src_dir, $(SRC_DIRS), $(wildcard $(src_dir)*.c))
SRCS_PP = $(foreach src_dir, $(SRC_DIRS), $(wildcard $(src_dir)*.cc))

TST_BLDS = $(patsubst $(TST_DIR)%.c, $(TST_BLD_DIR)%.out, $(TSTS))
RESULTS = $(patsubst $(TST_DIR)%.c, $(RES_DIR)%.txt, $(TSTS))
TST_BLD_RSCS = $(patsubst $(TST_DIR)%, $(TST_BLD_DIR)%, $(TST_RSCS))

OBJECTS = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.o, $(SRCS))
OBJECTS_NOMAIN = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.o, $(filter-out $(MAIN), $(SRCS)))

TST_BLD_DIRS = $(patsubst $(SRC_DIR)%/, $(TST_BLD_DIR)%/, $(SRC_DIRS))
RES_DIRS = $(patsubst $(SRC_DIR)%/, $(RES_DIR)%/, $(SRC_DIRS))

INCS=$(LEG_INC) $(RNT_DIR) $(COM_DIR) $(MSU_DIR)

CFLAGS+= $(foreach inc, $(INCS), -I$(inc))

define test_dep_name
$(notdir $(subst Test_,,$1))_DEPS
endef

define test_filters
$(subst Test_,, $(patsubst $(TST_BLD_DIR)%, $(OBJ_DIR)%.o, $1)) \
	$(foreach dep, $($(call test_dep_name, $1)), $(OBJ_DIR)$(dep:.c=.o))
endef

CCFLAGS=$(CFLAGS) $(CC_EXTRAFLAGS)
CPPFLAGS=$(CFLAGS) $(CPP_EXTRAFLAGS)

TEST_CFLAGS= $(CCFLAGS) -I$(TST_DIR) -lcheck_pic -lrt -lc -lpcap -lm -O0

DIRS = $(BLD_DIRS) $(OBJ_DIRS) $(DEP_DIRS) $(TST_BLD_DIRS) $(RES_DIRS)

define kill_proc
if pgrep -x "$1" > /dev/null; then killall "$1"; fi
endef

all: dirs legacy ${TARGET}

dirs: $(DIRS)

legacy: $(LEG_OBJ)

depends: $(DEP_DIRS) ${DEP_SRC}

$(LEG_BLD_DIR)%.o:: $(LEG_DIR)%
	@filename=$$(basename "$@"); filename="$${filename%.*}"; echo $$filename; cd $(LEG_DIR)/$$filename && make;
	@echo ___________ $< ___________
	$(LINK) -o $@ $</build/*.o

$(TARGET): ${OBJECTS} ${LEG_OBJ}
	$(FINAL) -o $@ $^ $(CFLAGS)

test: all test-blds test-results

test-blds: $(TST_BLDS) $(TST_BLD_RSCS)

test-results: all test-blds $(RESULTS)
	@echo "-----------------------\nTEST OUTPUT:\n-----------------------"
	@for FILE in $(filter-out all test-blds, $^); do \
		if grep -q ":[FE]:" "$$FILE"; then \
			echo ___ $$FILE ___ ; \
			cat $$FILE; \
		else \
			echo ___ $$FILE ___ ; \
			tail -1 $$FILE; \
		fi \
	done
	@echo "-----------------------\nFAILURES:\n-----------------------"
	@-grep -s ":F:" $^; echo "";
	@echo "-----------------------\nERRORS:\n-----------------------"
	@-grep -s ":E:" $^; echo "";
	@echo "\nDONE"

# Output the results of the tests by executing each of the builds
# of the tests. Output STDOUT and STDERR to the name of the rule
$(RES_DIR)%.txt: $(TST_BLD_DIR)%.out
	$(call kill_proc $(notdir $^))
	-./$< > $@ 2>&1

# creates the test executables by linking the test objects with the build objects excluding 
# the specific source under test
$(TST_BLD_DIR)%.out:: $(TST_DIR)%.c $(OBJECTS_NOMAIN) $(LEG_OBJ)
	$(FINAL_TEST) -o $@ $(filter-out $(call test_filters, $(@:.out=)), $^) $(TEST_CFLAGS)
#	$(FINAL) -o $@ $(filter-out $(subst Test_,, $(patsubst $(TST_BLD_DIR)%, $(OBJ_DIR)%.o, $@)), $^) $(TEST_CFLAGS)

$(TST_BLD_DIR)%: $(TST_DIR)%
	@cp $^ $@

# Creates object files from the source file
$(OBJ_DIR)%.o:: $(SRC_DIR)%.c $(SELF)
	$(COMPILE) $(CCFLAGS) $< -o $@

$(OBJ_DIR)%.o:: $(SRC_DIR)%.cc $(SELF)
	$(COMPILE_PP) $(CPPFLAGS) $< -o $@

$(DEP_SRC): $(DEP_DIRS)

$(DEP_DIR)%.d:: $(SRC_DIR)%.c $(LEG_OBJ)
	@$(DEPEND) $@ -MT $(patsubst $(DEP_DIR)%.d, $(OBJ_DIR)%.o, $@) $(TEST_CFLAGS) $<

$(DIRS)::
	@$(MKDIR) $@

clean:
	$(CLEAN_DIR) $(BLD_DIR)
	$(foreach leg_lib, $(LEG_LIBS), $(CLEAN_DIR) $(PATH_SRC_LEG)$(leg_lib)/build)
	$(CLEANUP) $(TARGET)

ifneq ($(MAKECMDGOALS), clean)
-include $(DEP_TST)
-include $(DEP_SRC)
-include $(TST_MKS)
endif

.PHONY: all
.PHONY: clean
.PHONY: test
.PHONY: legacy
.PHONY: depends
.PRECIOUS: $(DEP_DIR)%.d $(RES_DIR)%.txt $(OBJ_DIR)%.o $(TST_BLD_DIR)%.out
