TARGETS=global_controller runtime

TESTS=$(foreach TARG, $(TARGETS), $(TARG)-test)
CLEANS=$(foreach TARG, $(TARGETS), $(TARG)-clean)

all: $(TARGETS)

test: $(TESTS)

clean: $(CLEANS)

runtime-%::
	@make -f $(patsubst %-$*, %.mk, $@) $*

global_controller-%::
	@make -f $(patsubst %-$*, %.mk, $@) $*

$(TARGETS): FORCE
	@make -f $@.mk

FORCE:;
