CC=g++ -c
LD=ld -r

COM=common
RT=runtime
GC=global_controller

DIRS=$(COM) $(RT) $(GC)

all: $(DIRS)

rt: $(RT)

gc: $(GC)

$(DIRS): FORCE
	cd $@ && make;

clean:
	cd $(RT) && make clean;
	cd $(GC) && make clean;
	cd $(COM) && make clean;

.PHONY: clean all
FORCE:;
