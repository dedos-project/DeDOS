TARGETS= runtime global_controller

COVERAGE=$(foreach TARG, $(TARGETS), $(TARG)-coverage)
MEMCHECKS=$(foreach TARG, $(TARGETS), $(TARG)-memcheck)
TESTS=$(foreach TARG, $(TARGETS), $(TARG)-test)
CLEANS=$(foreach TARG, $(TARGETS), $(TARG)-clean)
UNITS=$(foreach TARG, $(TARGETS), $(TARG)-unit)

BLD_DIR = build/
RES_DIR = $(BLD_DIR)reults/
COV_DIR = $(BLD_DIR)test/coverage/

define tst_results
$(wildcard $(RES_DIR)*/*.txt)
endef

all: $(TARGETS)

test: $(TESTS)

memcheck: $(MEMCHECKS)

clean: $(CLEANS)

# A version of make test which runs only unit tests, not integration tests
unit: $(UNITS)

cov: $(COVERAGE)
	genhtml --show-details --keep-descriptions -o $(COV_DIR) $(shell find $(COV_DIR) -name '*.info' ! -empty)

coverage: $(COVERAGE)
	genhtml --show-details --keep-descriptions -o $(COV_DIR) $(shell find $(COV_DIR) -name '*.info' ! -empty)
	cd $(COV_DIR) && python2 -m SimpleHTTPServer 8081

docs:
	doxygen Doxyfile
	mv html docs

runtime-%::
	make -f $(patsubst %-$*, %.mk, $@) $*

global_controller-%::
	make -f $(patsubst %-$*, %.mk, $@) $*

$(TARGETS): FORCE
	make -f $@.mk

FORCE:;
