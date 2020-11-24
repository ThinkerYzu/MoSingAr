CFLAGS:= -fpic -Wall -Werror
BINS := test_ptracetools

all:: shellcode.o ptracetools.o $(BINS)

shellcode.o: shellcode.S
	$(CC) $(CFLAGS) -c $<

ptracetools.o: ptracetools.cpp
	$(CXX) $(CFLAGS) -c $<

test:: test_ptracetools
	./test_ptracetools

test_ptracetools: ptracetools.cpp shellcode.S
	$(CXX) -o $@ -DTEST ptracetools.cpp shellcode.S

clean::
	rm -f *.o *~ $(BINS)
