
# -fno-exceptions to avoid the symbol __gxx_personality_v0 of personality routine.
# see https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#base-personality
CFLAGS := -Wall -Werror -fpic -fvisibility=hidden -fno-builtin -nostdlib \
	-I./ -Ifake-inc/ -fno-builtin -I../loader -fno-exceptions

libtongdao_so_OBJS := bootstrap.o seccomp.o filter.o bridge.o \
	syscall-trampo.o tinylibc.o sig-trampo.o tinymalloc.o scout.o \
	../toolkits/msghelper.o

all:: libtongdao.so syscall-trampo.bin test_tinymalloc

libtongdao.so:  $(libtongdao_so_OBJS)
	$(CXX) -nostartfiles -shared -o $@ $(libtongdao_so_OBJS)

syscall-trampo.bin: syscall-trampo.o
	objcopy  -j .text -O binary syscall-trampo.o syscall-trampo.bin

syscall-trampo.o: syscall-trampo-x86_64.S
	$(CXX) $(CFLAGS) -c -o $@ $<

sig-trampo.o: sig-trampo-x86_64.S
	$(CXX) $(CFLAGS) -c -o $@ $<

seccomp.o: seccomp.cpp seccomp.h bridge.h
	$(CXX) $(CFLAGS) -c $<

filter.o: filter.cpp seccomp.h
	$(CXX) $(CFLAGS) -c $<

bridge.o: bridge.cpp bridge.h
	$(CXX) $(CFLAGS) -c $< -I../toolkits

bootstrap.o: bootstrap.cpp
	$(CXX) $(CFLAGS) -c $<

tinylibc.o: tinylibc.cpp
	$(CXX) $(CFLAGS) -c $<

tinymalloc.o: tinymalloc.cpp
	$(CXX) $(CFLAGS) -c $<

scout.o: scout.cpp scout.h
	$(CXX) $(CFLAGS) -c $< -I../toolkits

test_tinymalloc: tinymalloc.cpp
	$(CXX) -o $@ -DTEST -g tinymalloc.cpp

clean::
	rm -f *~ *.o libtongdao.so syscall-trampo.bin
