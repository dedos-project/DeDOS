CLEANUP=rm -f
CLEAN_DIR=rm -rf
MKDIR=mkdir -p
TARGET_EXT=out

PATH_SRC = src/
PATH_SRC_APP = $(PATH_SRC)apps/
PATH_SRC_MOD = $(PATH_SRC)modules/
PATH_SRC_STK = $(PATH_SRC)stack/
PATH_SRC_INC = $(PATH_SRC)include/
PATH_SRC_LEG = $(PATH_SRC)legacy/
PATH_TST = test/
PATH_BLD = build/
PATH_DEP = build/depends/
PATH_OBJ = build/objs/
PATH_BLD_LEG = build/legacy/
PATH_RES = build/results/

#LEGACY_LIBS =
LEGACY_LIBS = picotcp

COMPILE=gcc -c
COMPILE_PP=g++ -c
FINAL=gcc -lstdc++
LINK=ld -r
DEPEND=gcc -MM -MF

# If specific MSUs need special compilation instructions,
# they can be specified here, as such:
# COMPILE_dummy_msu=g++ -c

CFLAGS= -Wall -pthread -lpcre -lvdeplug -lssl -lcrypto -lm --std=gnu99 -g

LEGACY_MAKE=$(foreach leg_lib, $(LEGACY_LIBS), $(PATH_SRC_LEG)$(leg_lib)/Makefile)
LEGACY_INC=$(foreach leg_lib, $(LEGACY_LIBS), $(PATH_SRC_LEG)$(leg_lib)/build/include)
LEGACY_SRC=$(foreach leg_lib, $(LEGACY_LIBS, $(wildcard $(PATH_SRC_LEG)$(leg_lib)/src/*)))
LEGACY_OBJ=$(foreach leg_lib, $(LEGACY_LIBS), $(PATH_BLD_LEG)$(leg_lib).o)

INCLUDES= $(PATH_SRC_INC) $(LEGACY_INC)

CFLAGS+= $(foreach inc, $(INCLUDES), -I$(inc))

BUILD_PATHS = $(PATH_BLD) $(PATH_OBJ) $(PATH_RES) $(PATH_DEP) $(PATH_BLD_LEG)

SRC_TST = $(wildcard $(PATH_TST)*.c)

SRC_MOD = $(wildcard $(PATH_SRC_MOD)*.c)
SRC_MOD_PP = $(wildcard $(PATH_SRC_MOD)*.cc)
SRC_STK = $(wildcard $(PATH_SRC_STK)*.c)
SRC_ALL = $(SRC_MOD) $(SRC_MOD_PP) $(SRC_STK)

SRC_APP = $(wildcard $(PATH_SRC_APP)*.c)

RESULTS = $(patsubst $(PATH_TST)Test%.c, $(PATH_RES)Test%.txt, $(SRC_TST))

DEP_TST = $(patsubst $(PATH_TST)Test%.c, $(PATH_DEP)Test%.d, $(SRC_TST))
DEP_SRC = $(patsubst $(PATH_SRC_MOD)%.c, $(PATH_DEP)%.d, $(SRC_MOD)) \
		  $(patsubst $(PATH_SRC_MOD)%.cc, $(PATH_DEP)%.d, $(SRC_MOD_PP)) \
		  $(patsubst $(PATH_SRC_STK)%.c, $(PATH_DEP)%.d, $(SRC_STK))

OBJECTS = $(patsubst $(PATH_SRC_MOD)%.c, $(PATH_OBJ)%.o, $(SRC_MOD))
OBJECTS += $(patsubst $(PATH_SRC_MOD)%.cc, $(PATH_OBJ)%.o, $(SRC_MOD_PP))
OBJECTS += $(patsubst $(PATH_SRC_STK)%.c, $(PATH_OBJ)%.o, $(SRC_STK))
OBJECTS += $(patsubst $(PATH_SRC_UTL)%.c, $(PATH_OBJ)%.o, $(SRC_UTL))
APPS = $(patsubst $(PATH_SRC_APP)%.c, %, $(SRC_APP))

all: $(BUILD_PATHS) legacy $(APPS)

legacy: $(LEGACY_OBJ)

depends: $(DEP_SRC)


$(PATH_BLD_LEG)%.o:: $(PATH_SRC_LEG)%
	@filename=$$(basename "$@"); filename="$${filename%.*}"; echo $$filename; cd $(PATH_SRC_LEG)/$$filename && make;
	@echo ___________ $< ___________
	$(LINK) -o $@ $</build/*.o

%: $(OBJECTS) $(LEGACY_OBJ) $(PATH_SRC_APP)%.c
	$(FINAL) -o $@ $^ $(CFLAGS)


test: $(RESULTS)
	@echo "-----------------------\nTEST OUTPUT:\n-----------------------"
	@for FILE in "$^"; do echo ___ $$FILE ___ ; cat $$FILE; done
	@echo "-----------------------\nFAILURES:\n-----------------------"
	@echo `grep -s "F:" $(PATH_RES)*.txt`
	@echo "-----------------------\nERRORS:\n-----------------------"
	@echo `grep -s "E:" $(PATH_RES)*.txt`
	@echo "\nDONE"


# Output the results of the tests by executing each of the builds
# of the tests. Output STDOUT and STDERR to the name of the rule
$(PATH_RES)%.txt: $(PATH_BLD)%.$(TARGET_EXT)
	-./$< > $@ 2>&1


# creates the test executables by linking the test objects with
# the build objects, unity, and the test dependencies
$(PATH_BLD)Test%.$(TARGET_EXT): $(PATH_OBJ)Test%.o $(OBJECTS)
	$(LINK) -o $@ $^ $(CFLAGS)


# Creates object files from the test sources
$(PATH_OBJ)%.o:: $(PATH_TST)%.c
	$(COMPILE) $(CFLAGS) $< -o $@

# Creates object files from the source file
$(PATH_OBJ)%.o:: $(PATH_SRC_STK)%.c
	$(COMPILE) $(CFLAGS) $< -o $@

$(PATH_OBJ)%.o:: $(PATH_SRC_MOD)%.c
	@if [ "$(COMPILE_$(notdir $(@:.o=)))" = "" ]; then \
		echo $(COMPILE) $(CFLAGS) $< -o $@; \
		$(COMPILE) $(CFLAGS) $< -o $@; \
	else \
		echo $(COMPILE_$(notdir $(@:.o=))) $(CFLAGS) $< -o $@; \
		$(COMPILE_$(notdir $(@:.o=))) $(CFLAGS) $< -o $@; \
	fi

$(PATH_OBJ)%.o:: $(PATH_SRC_MOD)%.cc
	@if [ "$(COMPILE_$(notdir $(@:.o=)))" = "" ]; then \
		echo $(COMPILE_PP) $(CFLAGS) $< -o $@; \
		$(COMPILE_PP) $(CFLAGS) $< -o $@; \
	else \
		echo $(COMPILE_$(notdir $(@:.o=))) $(CFLAGS) $< -o $@; \
		$(COMPILE_$(notdir $(@:.o=))) $(CFLAGS) $< -o $@; \
	fi

$(PATH_DEP)%.d: $(PATH_TST)%.c
	$(DEPEND) $@ -MT $(patsubst $(PATH_DEP)%.d, $(PATH_OBJ)%.o, $@)  $(CFLAGS) $<

$(PATH_DEP)%.d: $(PATH_SRC_STK)%.c $(LEGACY_OBJ)
	$(DEPEND) $@ -MT $(patsubst $(PATH_DEP)%.d, $(PATH_OBJ)%.o, $@)  $(CFLAGS) $<

$(PATH_DEP)%.d: $(PATH_SRC_MOD)%.c $(LEGACY_OBJ)
	$(DEPEND) $@ -MT $(patsubst $(PATH_DEP)%.d, $(PATH_OBJ)%.o, $@)  $(CFLAGS) $<


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
.PRECIOUS: $(PATH_BLD)Test%.$(TARGET_EXT)
.PRECIOUS: $(PATH_OBJ)%.o
.PRECIOUS: $(PATH_BLD_LEG)%.o
.PRECIOUS: $(PATH_DEP)%.d
.PRECIOUS: $(PATH_RES)%.txt
