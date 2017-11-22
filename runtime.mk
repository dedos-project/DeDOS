ADDRESS_SANITIZER?=0
DEBUG = 0
DUMP_STATS = 1
DO_PROFILE = 0

LOGS = \
	   INFO \
	   ERROR \
	   WARN \
	   CRITICAL \
	   CUSTOM \

MSU_APPLICATIONS = webserver baremetal

SRC_DIR = src/
RNT_DIR = $(SRC_DIR)runtime/
MSU_DIR = $(SRC_DIR)msus/
COM_DIR = $(SRC_DIR)common/
LEG_DIR = $(SRC_DIR)legacy/
TST_DIR = test/
ITST_DIR = $(TST_DIR)/integration_tests

MSU_DIRS = $(MSU_DIR) $(foreach APP, $(MSU_APPLICATIONS), $(MSU_DIR)$(APP)/)

SRC_DIRS = $(RNT_DIR) $(COM_DIR) $(MSU_DIRS)

TARGET=rt
MAIN=$(RNT_DIR)main.c

BLD_DIR = build/
DEP_DIR = $(BLD_DIR)depends/
OBJ_DIR = $(BLD_DIR)objs/
TST_BLD_DIR = $(BLD_DIR)test/
RES_DIR = $(TST_BLD_DIR)reults/
COV_DIR = $(TST_BLD_DIR)coverage/
LEG_BLD_DIR =$(BLD_DIR)legacy/

BLD_DIRS = $(BLD_DIR) $(DEP_DIR) $(OBJ_DIR) $(RES_DIR) $(LEG_BLD_DIR)
BLD_DIRS += $(patsubst $(SRC_DIR)%/, $(OBJ_DIR)%/, $(SRC_DIRS))

CLEANUP=rm -f
CLEAN_DIR=rm -rf
MKDIR=mkdir -p

OPTIM=3

CC:=gcc
CXX:=g++

COMPILE=$(CC) -c
COMPILE_PP=$(CXX) -c
DEPEND=$(CC) -MM -MF
DEPEND_PP=$(CXX) -MM -MF
LINK=ld -r
FINAL=$(CXX)
FINAL_TEST=$(CXX)

SELF=./runtime.mk

define upper
$(shell echo $1 | tr a-z A-Z)
endef

LOG_DEFINES=$(foreach logname, $(LOGS), -DLOG_$(logname)) \
			$(foreach logname, $(NO_LOGS), -DNO_LOG_$(logname))
MSU_DEFINES=$(foreach MSU_APP, $(MSU_APPLICATIONS), -DCOMPILE_$(call upper, $(MSU_APP))_MSUS)

CFLAGS=-Wall -pthread -lssl -lrt -lcrypto -lm -O$(OPTIM) \
	   $(LOG_DEFINES) $(MSU_DEFINES)
CC_EXTRAFLAGS = --std=gnu99

ifneq (,$(findstring webserver,$(MSU_APPLICATIONS)))
CFLAGS+=-lssl -lpcre
endif

ifneq (,$(findstring ndlog,$(MSU_APPLICATIONS)))
CFLAGS+=-lvdeplug
endif

ifneq (,$(findstring pico_tcp,$(MSU_APPLICATIONS)))
LEGACY_LIBS+= picotcp
CFLAGS+=-lpcap
endif



ifeq ($(DEBUG), 1)
  CFLAGS+=-ggdb
endif

ifeq ($(DATAPLANE_PROFILING),1)
  CFLAGS+=-DDATAPLANE_PROFILING
endif

ifeq ($(ADDRESS_SANITIZER),1)
  CFLAGS+=-fsanitize=address -fno-omit-frame-pointer
endif

ifeq ($(DUMP_STATS), 1)
  CFLAGS+=-DDUMP_STATS=1
endif

ifeq ($(DO_PROFILE), 1)
  CFLAGS+=-DDEDOS_PROFILER
endif

ifneq (,$(findstring ndlog,$(MSU_APPLICATIONS)))
  CFLAGS+=-lgmp
endif

LEG_MAKE=$(foreach LEG_LIB, $(LEGACY_LIBS), $(LEG_DIR)$(LEG_LIB)/Makefile)
LEG_INC=$(foreach LEG_LIB, $(LEGACY_LIBS), $(LEG_DIR)$(LEG_LIB)/build/include)
LEG_SRC=$(foreach LEG_LIB, $(LEGACY_LIBS), $(wildcard $(LEG_DIR)$(LEG_LIB)/src/*))
LEG_OBJ=$(foreach LEG_LIB, $(LEGACY_LIBS), $(LEG_BLD_DIR)$(LEG_LIB).o)

RESOURCE_EXTS=.txt .json

UTST_DIRS = $(patsubst $(SRC_DIR)%/, $(TST_DIR)%/, $(SRC_DIRS))
TST_DIRS = $(UTST_DIRS) $(TST_DIR)integration_tests/

UTSTS = $(foreach TST_D, $(UTST_DIRS), $(wildcard $(TST_D)*.c))
TSTS = $(foreach TST_D, $(TST_DIRS), $(wildcard $(TST_D)*.c))
TST_MKS = $(foreach TST_D, $(TST_DIRS), $(wildcard $(TST_D)*.mk))
TST_RSCS = $(foreach TST_D, $(TST_DIRS), $(foreach EXT, $(RESOURCE_EXTS), $(wildcard $(TST_D)*$(EXT))))
TST_OBJS = $(patsubst $(TST_DIR)%.c, $(TST_BLD_DIR)%.o, $(TSTS))

SRCS = $(foreach src_dir, $(SRC_DIRS), $(wildcard $(src_dir)*.c))
SRCS_PP = $(foreach src_dir, $(SRC_DIRS), $(wildcard $(src_dir)*.cc))

TST_BLDS = $(patsubst $(TST_DIR)%.c, $(TST_BLD_DIR)%.out, $(TSTS))
TST_COV = $(patsubst $(TST_DIR)%.c, $(TST_BLD_DIR)%.gcda, $(TSTS)) \
		  $(patsubst $(TST_DIR)%.c, $(TST_BLD_DIR)%.gcno, $(TSTS))
TST_COV_INFOS = $(patsubst $(TST_DIR)%.c, $(TST_BLD_DIR)%.gcno, $(TSTS))

COV_DIRS = $(sort $(dir $(patsubst $(TST_DIR)%/, $(COV_DIR)%, $(TST_DIRS)) $(COV_DIR)))
COV_INFOS = $(patsubst $(TST_DIR)%/, $(COV_DIR)%.info, $(TST_DIRS))
COV_INIT_INFOS = $(patsubst $(TST_DIR)%/, $(COV_DIR)%.init_info, $(TST_DIRS))
COV_INDEX = $(COV_DIR)index.html

RESULTS = $(patsubst $(TST_DIR)%.c, $(RES_DIR)%.txt, $(TSTS))
UNIT_RESULTS = $(patsubst $(TST_DIR)%.c, $(RES_DIR)%.txt, $(UTSTS))
MEM_RESULTS = $(patsubst $(TST_DIR)%.c, $(RES_DIR)%_memcheck.txt, $(TSTS))
TST_BLD_RSCS = $(patsubst $(TST_DIR)%, $(TST_BLD_DIR)%, $(TST_RSCS))

DEP_DIRS = $(patsubst $(TST_DIR)%/, $(DEP_DIR)%/, $(TST_DIRS))
DEP_TST = $(patsubst $(TST_DIR)%.c, $(DEP_DIR)%.d, $(TSTS))
DEP_SRC = $(patsubst $(SRC_DIR)%.c, $(DEP_DIR)%.d, $(SRCS)) \
		  $(patsubst $(SRC_DIR)%.cc, $(DEP_DIR)%.d, $(SRCS_PP))
OBJECTS = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.o, $(SRCS)) \
		  $(patsubst $(SRC_DIR)%.cc, $(OBJ_DIR)%.o, $(SRCS_PP))
OBJECTS_NOMAIN = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.o, $(filter-out $(MAIN), $(SRCS))) \
				 $(patsubst $(SRC_DIR)%.cc, $(OBJ_DIR)%.o, $(SRCS_PP))

TST_BLD_DIRS = $(patsubst $(TST_DIR)%/, $(TST_BLD_DIR)%/, $(TST_DIRS))
RES_DIRS = $(patsubst $(TST_DIR)%/, $(RES_DIR)%/, $(TST_DIRS))

INCS=$(LEG_INC) $(RNT_DIR) $(COM_DIR) $(MSU_DIR)
CFLAGS+= $(foreach inc, $(INCS), -I$(inc))

define test_dep_name
$(notdir $(subst Test_,,$1))_DEPS
endef

define test_filters
$(subst Test_,, $(patsubst $(TST_BLD_DIR)%, $(OBJ_DIR)%.o, $1)) \
	$(foreach dep, $($(call test_dep_name, $1)), $(OBJ_DIR)$(dep:.c=.o))
endef

TEST_CFLAGS= $(CFLAGS) $(CC_EXTRAFLAGS) -I$(TST_DIR) -O0 -lcheck_pic 
ifeq ($(MAKECMDGOALS),$(filter $(MAKECMDGOALS),coverage init_cov cov-site cov))
  CFLAGS+= -fprofile-arcs -ftest-coverage --coverage
  OPTIM=0
  INIT_COV=init_cov
  TEST_CFLAGS+=-fprofile-arcs -ftest-coverage --coverage
endif

CCFLAGS=$(CFLAGS) $(CC_EXTRAFLAGS)
CPPFLAGS=$(CFLAGS) $(CPP_EXTRAFLAGS)

DIRS = $(BLD_DIRS) $(OBJ_DIRS) $(DEP_DIRS) $(TST_BLD_DIRS) $(RES_DIRS) $(COV_DIRS)


all: dirs legacy ${TARGET}

dirs: $(DIRS)
	echo $(LEG_DIR)
	echo $(LEG_BLD_DIR)

legacy: $(LEG_OBJ)

depends: $(DEP_DIRS) ${DEP_SRC}

coverage: $(DIRS) $(OBJECTS) $(COV_INIT_INFOS)  test $(TST_COV) $(COV_INFOS)

cov: coverage
	genhtml --show-details -o $(COV_DIR) $(shell find $(COV_DIR) -name '*.info' ! -empty)
	mv $(COV_DIR) coverage

cov-site: coverage
	genhtml --show-details -o $(COV_DIR) $(shell find $(COV_DIR) -name '*.info' ! -empty)
	cd $(COV_DIR) && python2 -m SimpleHTTPServer 8081

$(TST_BLD_DIR)%.gcda: $(TST_BLD_DIR)%.out $(DIRS)

init_cov: $(DIRS) $(OBJECTS) $(COV_INIT_INFOS)

lcov: $(DIRS) $(TST_COV) $(COV_INFOS)

$(COV_DIR)%.init_info: $(TST_BLD_DIR)%/
	-lcov --no-recursion --directory $(patsubst $(TST_BLD_DIR)%,$(OBJ_DIR)%,$<) --zerocounters
	-lcov --no-recursion --directory $(patsubst $(TST_BLD_DIR)%,$(OBJ_DIR)%,$<) --capture --initial --output-file $@ --test-name $(notdir $(subst .init_info,,$@));

$(COV_DIR)%.info: $(TST_BLD_DIR)%/ test-results
	-lcov --external --directory $< --capture --no-recursion --output-file $(subst .info,.raw_info,$@) --test-name $(notdir $(subst .info,,$@))
	-@if [ -e $(subst .info,.init_info,$@) ]; then \
		lcov --external -a $(subst .info,.init_info,$@) -a $(subst .info,.raw_info,$@)  -o $(subst .info,.all_info,$@) --test-name $(notdir $(subst .info,,$@)); \
	else \
		lcov --external -a $(subst .info,.raw_info,$@) -o $(subst .info,.all_info,$@) --test-name $(notdir $(subst .info,,$@)); \
	fi
	-lcov --remove $(subst .info,.all_info,$@) '/test/*' '/usr/*' 'src/legacy/*' -o $@  --test-name $(notdir $(subst .info,,$@))

$(LEG_OBJ): $(LEG_BLD_DIR)
	@echo ___________ $< ___________
	@filename=$$(basename "$@"); filename="$${filename%.*}"; echo $$filename; cd $(LEG_DIR)/$$filename && make;
	$(LINK) -o $@ src/legacy/$(patsubst %.o,%,$(notdir $(LEG_OBJ)))/build/*.o

$(TARGET): ${OBJECTS} ${LEG_OBJ}
	$(FINAL) -o $@ $^ $(CFLAGS)

unit: all ${INIT_COV} test-blds utest-results

test: all ${INIT_COV} test-blds test-results

memcheck: test $(MEM_RESULTS)
	@export CK_DEFAULT_TIMEOUT=30; \
	for FILE in $(filter-out test, $^); do \
		echo ___ $$FILE ___ ; \
		if grep -q "definitely lost: [^0]" "$$FILE"; then \
			grep  --color=never '\(\*\*\*\*\*.*\*\*\*\*\)\|\(definitely lost: [^0]\)' "$$FILE" | sed s/==/`printf "\\033[032m"==`/g ;\
			tput sgr0; \
		else \
			echo "Nothing definitely lost!"; \
		fi \
	done;


test-blds: $(TST_OBJS) $(TST_BLDS) $(TST_BLD_RSCS)

define show_results
	@echo "-----------------------\nTEST OUTPUT:\n-----------------------"
	@for FILE in $1; do \
		if grep -q ":[FES]:" "$$FILE"; then \
			echo ___ $$FILE ___ ; \
			cat $$FILE; \
		elif grep -q "Aborted" "$$FILE"; then \
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
	@if grep -q ":[FE]:" $1; then \
		false;\
	fi
endef

utest-results: all test-blds $(UNIT_RESULTS)
	$(call show_results, $(filter-out all test-blds, $^))

test-results: all test-blds $(RESULTS)
	$(call show_results, $(filter-out all test-blds, $^))

$(RES_DIR)%_memcheck.txt: $(TST_BLD_DIR)%.out
	-valgrind  --track-origins=yes --leak-check=full $< > $@ 2>&1

# Output the results of the tests by executing each of the builds
# of the tests. Output STDOUT and STDERR to the name of the rule
$(RES_DIR)%.txt: $(TST_BLD_DIR)%.out FORCE
	@killall $(notdir $<) 2> /dev/null || true
	-./$< > $@ 2>&1

$(TST_BLD_DIR)%.o:: $(TST_DIR)%.c $(SELF) 
	$(COMPILE) $(TEST_CFLAGS) $< -o $@

# creates the test executables by linking the test objects with the build objects excluding 
# the specific source under test
$(TST_BLD_DIR)%.out:: $(TST_BLD_DIR)%.o $(OBJECTS_NOMAIN) $(LEG_OBJ)
	$(FINAL_TEST) -o $@ $(filter-out $(call test_filters, $(@:.out=)), $^) $(TEST_CFLAGS)
#	$(FINAL) -o $@ $(filter-out $(subst Test_,, $(patsubst $(TST_BLD_DIR)%, $(OBJ_DIR)%.o, $@)), $^) $(TEST_CFLAGS)

$(TST_BLD_DIR)%: $(TST_DIR)% FORCE
	-@cp $< $@

# Creates object files from the source file
$(OBJ_DIR)%.o:: $(SRC_DIR)%.c $(SELF)
	$(COMPILE) $(CCFLAGS) $< -o $@

$(OBJ_DIR)%.o:: $(SRC_DIR)%.cc $(SELF)
	$(COMPILE_PP) $(CPPFLAGS) $< -o $@

$(DEP_SRC): $(DEP_DIRS)

$(DEP_DIR)%.d:: $(SRC_DIR)%.cc $(LEG_OBJ)
	@$(DEPEND) $@ -MT $(patsubst $(DEP_DIR)%.d, $(OBJ_DIR)%.o, $@) $(CPPFLAGS) $<

$(DEP_DIR)%.d:: $(SRC_DIR)%.c $(LEG_OBJ)
	@$(DEPEND) $@ -MT $(patsubst $(DEP_DIR)%.d, $(OBJ_DIR)%.o, $@) $(TEST_CFLAGS) $<

$(DEP_DIR)%.d:: $(TST_DIR)%.c $(LEG_OBJ)
	@$(DEPEND) $@ -MT $(patsubst $(DEP_DIR)%.d, $(TST_BLD_DIR)%.o, $@) $(TEST_CFLAGS) $<

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
.PHONY: coverage
.PRECIOUS: $(DEP_DIR)%.d $(RES_DIR)%.txt $(OBJ_DIR)%.o $(TST_BLD_DIR)%.out
FORCE:;

