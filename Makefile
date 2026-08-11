.PHONY: all diagnostic clean verify

all: diagnostic

diagnostic:
	$(MAKE) -C diagnostic

verify:
	$(MAKE) -C diagnostic verify

clean:
	$(MAKE) -C diagnostic clean

