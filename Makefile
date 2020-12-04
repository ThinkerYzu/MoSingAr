
.PHONY: all sandbox loader toolkits clean test

all: sandbox loader

sandbox: toolkits
	$(MAKE) -C $@

loader: toolkits
	$(MAKE) -C $@

toolkits:
	$(MAKE) -C $@

test:
	$(MAKE) -C loader test
	$(MAKE) -C sandbox test
	$(MAKE) -C toolkits test

clean:
	$(MAKE) -C loader clean
	$(MAKE) -C sandbox clean
	$(MAKE) -C toolkits clean
