
# -fno-exceptions to avoid the symbol __gxx_personality_v0 of personality routine.
# see https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#base-personality
CFLAGS := -Wall -Werror -fpic -fvisibility=hidden -fno-builtin -nostdlib \
	-I./ -Ifake-inc/ -fno-builtin -I../loader -fno-exceptions -I../toolkits \
	-fno-stack-protector

libmosingar_so_OBJS := bootstrap.o seccomp.o filter.o bridge.o \
	syscall-trampo.o tinylibc.o sig-trampo.o tinymalloc.o scout.o \
	../toolkits/msghelper.o fakeframe-trampo.o

.PHONY: all clean test

all:: libmosingar.so syscall-trampo.bin test_tinymalloc

libmosingar.so:  $(libmosingar_so_OBJS)
	$(CXX) -nostartfiles -shared -o $@ $(libmosingar_so_OBJS)

syscall-trampo.bin: syscall-trampo.o
	objcopy  -j .text -O binary syscall-trampo.o syscall-trampo.bin

syscall-trampo.o: syscall-trampo-x86_64.S
	$(CXX) $(CFLAGS) -c -o $@ $<

fakeframe-trampo.o: fakeframe-trampoline-x86_64.S
	$(CXX) $(CFLAGS) -c -o $@ $<

sig-trampo.o: sig-trampo-x86_64.S
	$(CXX) $(CFLAGS) -c -o $@ $<

seccomp.o: seccomp.cpp seccomp.h bridge.h
	$(CXX) $(CFLAGS) -c $<

filter.o: filter.cpp seccomp.h
	$(CXX) $(CFLAGS) -c $<

bridge.o: bridge.cpp bridge.h
	$(CXX) $(CFLAGS) -c $<

bootstrap.o: bootstrap.cpp
	$(CXX) $(CFLAGS) -c $<

tinylibc.o: tinylibc.cpp
	$(CXX) $(CFLAGS) -c $<

tinymalloc.o: tinymalloc.cpp
	$(CXX) $(CFLAGS) -c $<

scout.o: scout.cpp scout.h
	$(CXX) $(CFLAGS) -c $<

test_tinymalloc: tinymalloc.cpp
	$(CXX) -o $@ -DTEST -g tinymalloc.cpp

clean::
	rm -f *~ *.o libmosingar.so syscall-trampo.bin test_tinymalloc
	$(MAKE) -C tests clean

test:
	$(MAKE) -C tests test
