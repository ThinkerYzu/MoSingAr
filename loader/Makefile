CFLAGS:= -fpic -Wall -Werror
BINS := test_ptracetools test_flightdeck
LIBS := libloader.so

libloader_so_OBJS := ptracetools.o shellcode.o loader.o flightdeck.o cmdcenter.o scout.o

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

cmdcenter.o: cmdcenter.cpp cmdcenter.h
	$(CXX) $(CFLAGS) -c $<

scout.o: scout.cpp scout.h cmdcenter.h
	$(CXX) $(CFLAGS) -c $<

test_flightdeck: flightdeck.cpp ptracetools.o shellcode.o loader.o
	$(CXX) -DTEST -o $@ $< ptracetools.o shellcode.o loader.o

test:: test_ptracetools
	./test_ptracetools

test_ptracetools: ptracetools.cpp shellcode.S
	$(CXX) -o $@ -DTEST ptracetools.cpp shellcode.S

clean::
	rm -f *.o *~ $(BINS) $(LIBS)
