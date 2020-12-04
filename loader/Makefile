CFLAGS:= -fpic -Wall -Werror -g -I../toolkits
BINS := test_ptracetools test_flightdeck carrier
LIBS := libloader.so

libloader_so_OBJS := ptracetools.o shellcode.o loader.o flightdeck.o \
	carrier.o cmdcenter.o ../toolkits/msghelper.o

all:: $(BINS) $(LIBS)

libloader.so: $(libloader_so_OBJS)
	$(CXX) $(CFLAGS) -shared -o $@ $(libloader_so_OBJS)

shellcode.o: shellcode.S
	$(CC) $(CFLAGS) -fvisibility=hidden -nostdlib -c $<

loader.o: loader.cpp
	$(CXX) $(CFLAGS) -fvisibility=hidden -nostdlib -fno-builtin -c $<

ptracetools.o: ptracetools.cpp
	$(CXX) $(CFLAGS) -c $<

flightdeck.o: flightdeck.cpp
	$(CXX) $(CFLAGS) -c $<

carrier.o: carrier.cpp
	$(CXX) $(CFLAGS) -c $<

carrier: main.cpp libloader.so
	$(CXX) -g -o $@ main.cpp libloader.so -I../toolkits

cmdcenter.o: cmdcenter.cpp cmdcenter.h flightdeck.h
	$(CXX) $(CFLAGS) -c $< -I../sandbox

test_flightdeck: flightdeck.cpp ptracetools.o shellcode.o loader.o
	$(CXX) -DTEST -o $@ $< ptracetools.o shellcode.o loader.o -I../toolkits

test:: test_ptracetools test_flightdeck carrier
	./test_ptracetools
	@echo
	../sandbox/tests/fake_cc ./test_flightdeck
	@echo
	LD_LIBRARY_PATH=../sandbox:./ ./carrier ./tests/hello

test_ptracetools: ptracetools.cpp shellcode.S
	$(CXX) -o $@ -DTEST ptracetools.cpp shellcode.S

clean::
	rm -f *.o *~ $(BINS) $(LIBS)
