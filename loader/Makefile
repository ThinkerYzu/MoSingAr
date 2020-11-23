CFLAGS:= -fpic -Wall -Werror

all: shellcode.o ptracetools.o

shellcode.o: shellcode.S
	$(CC) $(CFLAGS) -c $<

ptracetools.o: ptracetools.cpp
	$(CXX) $(CFLAGS) -c $<

clean::
	rm -f *.o *~
