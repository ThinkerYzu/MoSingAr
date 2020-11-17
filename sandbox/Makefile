
CFLAGS := -Wall -Werror -fpic

all:: libtongdao.so syscall-trampo.bin

libtongdao.so: seccomp.o filter.o bridge.o bootstrap.o syscall-trampo.o
	$(CXX) -shared -o $@ syscall-trampo.o seccomp.o filter.o bridge.o bootstrap.o

syscall-trampo.bin: syscall-trampo.o
	objcopy  -j .text -O binary syscall-trampo.o syscall-trampo.bin

syscall-trampo.o: syscall-trampo-x86_64.S
	gcc -c -fpic -o $@ $<

seccomp.o: seccomp.cpp seccomp.h bridge.h
	$(CXX) $(CFLAGS) -c $<

filter.o: filter.cpp seccomp.h
	$(CXX) $(CFLAGS) -c $<

bridge.o: bridge.cpp bridge.h
	$(CXX) $(CFLAGS) -c $< -I../toolkits

bootstrap.o: bootstrap.cpp
	$(CXX) $(CFLAGS) -c $<

clean::
	rm -f *~ *.o libtongdao.so syscall-trampo.bin
