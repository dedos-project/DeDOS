TARGETS=global_controller runtime

TESTS=$(foreach TARG, $(TARGETS), $(TARG)-test)
CLEANS=$(foreach TARG, $(TARGETS), $(TARG)-clean)

BLD_DIR = build/
RES_DIR = $(BLD_DIR)reults/

define tst_results
$(wildcard $(RES_DIR)*/*.txt)
endef

all: $(TARGETS)

test: $(TESTS) test-results

clean: $(CLEANS)

runtime-%::
	make -f $(patsubst %-$*, %.mk, $@) $*

global_controller-%::
	make -f $(patsubst %-$*, %.mk, $@) $*

$(TARGETS): FORCE
	make -f $@.mk

test-results: $(TESTS) 
	@echo "-----------------------\nALL TEST OUTPUTS:\n-----------------------"
	@for FILE in $(call tst_results); do \
		if grep -q ":[FE]:" "$$FILE"; then \
			echo ___ $$FILE ___ ; \
			cat $$FILE; \
		else \
			echo ___ $$FILE ___ ; \
			tail -1 $$FILE; \
		fi \
	done
	@echo "-----------------------\nALL FAILURES:\n-----------------------"
	@-grep -s ":F:" $(call tst_results); echo "";
	@echo "-----------------------\nALL ERRORS:\n-----------------------"
	@-grep -s ":E:" $(call tst_results); echo "";
	@echo "\nDONE"

FORCE:;
