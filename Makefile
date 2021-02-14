
.PHONY: all sandbox loader toolkits clean test 3rd-parties oglfs

all: 3rd-parties oglfs sandbox loader

sandbox: toolkits
	$(MAKE) -C $@

loader: toolkits
	$(MAKE) -C $@

toolkits:
	$(MAKE) -C $@

3rd-parties:
	$(MAKE) -C $@

oglfs:
	$(MAKE) -C $@

test:
	$(MAKE) -C 3rd-parties test
	$(MAKE) -C oglfs test
	$(MAKE) -C loader test
	$(MAKE) -C sandbox test
	$(MAKE) -C toolkits test

clean:
	$(MAKE) -C 3rd-parties clean
	$(MAKE) -C oglfs clean
	$(MAKE) -C loader clean
	$(MAKE) -C sandbox clean
	$(MAKE) -C toolkits clean
	$(MAKE) -C playground clean
