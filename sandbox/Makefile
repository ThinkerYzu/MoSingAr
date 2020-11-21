
CFLAGS := -Wall -Werror -fpic -fvisibility=hidden -fno-builtin -nostdlib -I./ -fno-builtin

libtongdao_so_OBJS := bootstrap.o seccomp.o filter.o bridge.o \
	syscall-trampo.o tinylibc.o sig-trampo.o tinymalloc.o

all:: libtongdao.so syscall-trampo.bin

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

clean::
	rm -f *~ *.o libtongdao.so syscall-trampo.bin
