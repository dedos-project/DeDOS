CLEANUP=rm -f
CLEAN_DIR=rm -rf
MKDIR=mkdir -p
TARGET_EXT=out

LOGS = \
	   INFO \
	   ERROR \
	   WARN \
	   CRITICAL \
	   CUSTOM \
	   #DEBUG \
	   WEBSERVER_READ \
	   EPOLL_OPS \
	   WEBSERVER_ROUTING \
	   MSU_STATE_MANAGEMENT

DATAPLANE_PROFILING=0
DEBUG = 1
COLLECT_STATS = 1
DUMP_STATS = 1

PATH_SRC = src/
PATH_SRC_APP = $(PATH_SRC)apps/
PATH_SRC_MOD = $(PATH_SRC)modules/
PATH_SRC_WBS = $(PATH_SRC_MOD)webserver/
PATH_SRC_STK = $(PATH_SRC)stack/
PATH_SRC_INC = $(PATH_SRC)include/
PATH_SRC_LEG = $(PATH_SRC)legacy/
PATH_TST = test/
PATH_BLD = build/
PATH_RSC = build/resources/
PATH_DEP = build/depends/
PATH_OBJ = build/objs/
PATH_BLD_LEG = build/legacy/
PATH_RES = build/results/
ADDRESS_SANITIZER?=0

COMMON = ../common/
COMMON_SRC = $(COMMON)src/
COMMON_BLD = $(COMMON)build/
COMMON_OBJ = $(COMMON_BLD)objs/

COMMON_SRCS = $(wildcard $(COMMON_SRC)*.c)
COMMON_OBJS = $(patsubst $(COMMON_SRC)%.c, $(COMMON_OBJ)%.o, $(COMMON_SRCS))

#LEGACY_LIBS =
LEGACY_LIBS = picotcp

COMPILE=gcc -c
# -O3
COMPILE_PP=g++ -c
FINAL=gcc
LINK=ld -r
DEPEND=gcc -MM -MF

SELF=./Makefile

# If specific MSUs need special compilation instructions,
# they can be specified here, as such:
CFLAGS_dummy_msu= -lpcre

LOG_DEFINES=$(foreach logname, $(LOGS), -DLOG_$(logname)=1)
LOG_DEFINES+=$(foreach logname, $(DISABLED_LOGS), -DLOG_$(logname)=0)

CFLAGS= -Wall -pthread -lpcre -lvdeplug -lssl -lcrypto -lm -lpcap --std=gnu99 $(LOG_DEFINES)

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

LEGACY_MAKE=$(foreach leg_lib, $(LEGACY_LIBS), $(PATH_SRC_LEG)$(leg_lib)/Makefile)
LEGACY_INC=$(foreach leg_lib, $(LEGACY_LIBS), $(PATH_SRC_LEG)$(leg_lib)/build/include)
LEGACY_SRC=$(foreach leg_lib, $(LEGACY_LIBS, $(wildcard $(PATH_SRC_LEG)$(leg_lib)/src/*)))
LEGACY_OBJ=$(foreach leg_lib, $(LEGACY_LIBS), $(PATH_BLD_LEG)$(leg_lib).o)

INCLUDES= $(PATH_SRC_INC) $(LEGACY_INC) $(COMMON_SRC)

CFLAGS+= $(foreach inc, $(INCLUDES), -I$(inc))

TEST_CFLAGS= $(CFLAGS) -I$(PATH_SRC_STK) -lcheck_pic -lrt -lc -lpcap

BUILD_PATHS = $(PATH_BLD) $(PATH_OBJ) $(PATH_RES) $(PATH_DEP) $(PATH_BLD_LEG)

SRC_TST = $(wildcard $(PATH_TST)*.c)

SRC_MOD = $(wildcard $(PATH_SRC_MOD)*.c)
SRC_MOD_PP = $(wildcard $(PATH_SRC_MOD)*.cc)
SRC_WBS = $(wildcard $(PATH_SRC_WBS)*.c)
SRC_STK = $(wildcard $(PATH_SRC_STK)*.c)
SRC_ALL = $(SRC_MOD) $(SRC_MOD_PP) $(SRC_STK)

SRC_APP = $(wildcard $(PATH_SRC_APP)*.c)

RESULTS = $(patsubst $(PATH_TST)Test_%.c, $(PATH_RES)Test_%.txt, $(SRC_TST))
RESOURCE_IN = $(wildcard $(PATH_TST)resources/*)
TEST_RESOURCES = $(patsubst $(PATH_TST)resources/%, $(PATH_RSC)%, $(RESOURCE_IN))

DEP_TST = $(patsubst $(PATH_TST)Test_%.c, $(PATH_DEP)Test_%.d, $(SRC_TST))
DEP_SRC = $(patsubst $(PATH_SRC_MOD)%.c, $(PATH_DEP)%.d, $(SRC_MOD)) \
		  $(patsubst $(PATH_SRC_MOD)%.cc, $(PATH_DEP)%.d, $(SRC_MOD_PP)) \
		  $(patsubst $(PATH_SRC_WBS)%.c, $(PATH_DEP)%.d, $(SRC_MOD_PP)) \
		  $(patsubst $(PATH_SRC_STK)%.c, $(PATH_DEP)%.d, $(SRC_STK))

OBJECTS = $(patsubst $(PATH_SRC_MOD)%.c, $(PATH_OBJ)%.o, $(SRC_MOD))
OBJECTS += $(patsubst $(PATH_SRC_MOD)%.cc, $(PATH_OBJ)%.o, $(SRC_MOD_PP))
OBJECTS += $(patsubst $(PATH_SRC_WBS)%.c, $(PATH_OBJ)%.o, $(SRC_WBS))
OBJECTS += $(patsubst $(PATH_SRC_STK)%.c, $(PATH_OBJ)%.o, $(SRC_STK))
OBJECTS += $(patsubst $(PATH_SRC_UTL)%.c, $(PATH_OBJ)%.o, $(SRC_UTL))
APPS = $(patsubst $(PATH_SRC_APP)%.c, %, $(SRC_APP))

define cflags
	$(CFLAGS) $(value CFLAGS_$(notdir $(patsubst %.o, %, $(1))))
endef


all: $(BUILD_PATHS) legacy $(APPS)

legacy: $(LEGACY_OBJ)

depends: $(DEP_SRC)


$(PATH_BLD_LEG)%.o:: $(PATH_SRC_LEG)%
	@filename=$$(basename "$@"); filename="$${filename%.*}"; echo $$filename; cd $(PATH_SRC_LEG)/$$filename && make;
	@echo ___________ $< ___________
	$(LINK) -o $@ $</build/*.o

%: $(OBJECTS) $(LEGACY_OBJ) $(COMMON_OBJS) $(PATH_SRC_APP)%.c
	$(FINAL) -o $@ $^ $(CFLAGS)

$(COMMON_OBJ)%.o: $(COMMON_SRC)%.c
	cd .. && make common

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

$(TEST_RESOURCES): $(RESOURCE_IN)
	mkdir -p $(PATH_BLD)/resources
	cp -r $^ $(PATH_BLD)/resources

# Output the results of the tests by executing each of the builds
# of the tests. Output STDOUT and STDERR to the name of the rule
$(PATH_RES)%.txt: $(PATH_BLD)%.$(TARGET_EXT) $(TEST_RESOURCES)
	-./$< > $@ 2>&1


# creates the test executables by linking the test objects with
# the build objects, unity, and the test dependencies
$(PATH_BLD)Test_%.$(TARGET_EXT): $(PATH_OBJ)Test_%.o $(OBJECTS) $(LEGACY_OBJ) $(COMMON_OBJS)
	$(FINAL) -o $@ $(filter-out $(patsubst $(PATH_BLD)Test_%.$(TARGET_EXT), $(PATH_OBJ)%.o, $@), $^) $(TEST_CFLAGS)


# Creates object files from the test sources
$(PATH_OBJ)%.o:: $(PATH_TST)%.c $(SELF)
	$(COMPILE) $(TEST_CFLAGS) $< -o $@

# Creates object files from the stack source file
$(PATH_OBJ)%.o:: $(PATH_SRC_STK)%.c $(SELF)
	$(COMPILE) $(CFLAGS) $< -o $@

$(PATH_OBJ)%.o:: $(PATH_SRC_WBS)%.c $(SELF)
	$(COMPILE) $(CFLAGS) $< -o $@

$(PATH_OBJ)%.o:: $(PATH_SRC_MOD)%.c $(SELF)
	$(COMPILE) $(call cflags, $@) $< -o $@

$(PATH_OBJ)%.o:: $(PATH_SRC_MOD)%.cc $(SELF)
	$(COMPILE_PP) $(call cflags, $@) $< -o $@

$(PATH_DEP)%.d: $(PATH_TST)%.c
	@$(DEPEND) $@ -MT $(patsubst $(PATH_DEP)%.d, $(PATH_OBJ)%.o, $@)  $(TEST_CFLAGS) $<

$(PATH_DEP)%.d: $(PATH_SRC_STK)%.c $(LEGACY_OBJ)
	@$(DEPEND) $@ -MT $(patsubst $(PATH_DEP)%.d, $(PATH_OBJ)%.o, $@)  $(CFLAGS) $<

$(PATH_DEP)%.d: $(PATH_SRC_MOD)%.c $(LEGACY_OBJ)
	@$(DEPEND) $@ -MT $(patsubst $(PATH_DEP)%.d, $(PATH_OBJ)%.o, $@)  $(CFLAGS) $<


$(PATH_BLD):
	$(MKDIR) $(PATH_BLD)

$(PATH_OBJ):
	$(MKDIR) $(PATH_OBJ)

$(PATH_RES):
	$(MKDIR) $(PATH_RES)

$(PATH_BLD_LEG):
	$(MKDIR) $(PATH_BLD_LEG)

$(PATH_DEP):
	$(MKDIR) $(PATH_DEP)

clean:
	$(CLEANUP) $(PATH_OBJ)*.o
	$(CLEANUP) $(PATH_BLD)*.$(TARGET_EXT)
	$(CLEANUP) $(PATH_BLD_LEG)*.o
	$(foreach leg_lib, $(LEGACY_LIBS), $(CLEAN_DIR) $(PATH_SRC_LEG)$(leg_lib)/build)
	$(CLEANUP) $(PATH_RES)*.txt
	$(CLEANUP) $(PATH_DEP)*.d
	$(CLEANUP) $(APPS)

ifneq ($(MAKECMDGOALS), clean)
-include $(DEP_TST)
-include $(DEP_SRC)
endif

.PHONY: all
.PHONY: clean
.PHONY: test
.PHONY: legacy
.PHONY: depends
.PRECIOUS: $(PATH_BLD)Test_%.$(TARGET_EXT)
.PRECIOUS: $(PATH_OBJ)%.o
.PRECIOUS: $(PATH_BLD_LEG)%.o
.PRECIOUS: $(PATH_DEP)%.d
.PRECIOUS: $(PATH_RES)%.txt
