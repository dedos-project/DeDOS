TARGETS=runtime global_controller

MEMCHECKS=$(foreach TARG, $(TARGETS), $(TARG)-memcheck)
TESTS=$(foreach TARG, $(TARGETS), $(TARG)-test)
CLEANS=$(foreach TARG, $(TARGETS), $(TARG)-clean)

BLD_DIR = build/
RES_DIR = $(BLD_DIR)reults/

define tst_results
$(wildcard $(RES_DIR)*/*.txt)
endef

all: $(TARGETS)

test: $(TESTS)

memcheck: $(MEMCHECKS)

clean: $(CLEANS)

runtime-%::
	make -f $(patsubst %-$*, %.mk, $@) $*

global_controller-%::
	make -f $(patsubst %-$*, %.mk, $@) $*

$(TARGETS): FORCE
	make -f $@.mk

FORCE:;
