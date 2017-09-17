TARGETS=runtime global_controller



all: $(TARGETS)

$(TARGETS): FORCE
	@make -f $@.mk

test: FORCE
	$(foreach T, $(TARGETS), make -f $(T).mk test;)

clean: FORCE
	$(foreach T, $(TARGETS), make -f $(T).mk clean;)


FORCE:;
