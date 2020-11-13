
all:: seccomp.o filter.o

seccomp.o: seccomp.cpp seccomp.h
	$(CXX) -c $<

filter.o: filter.cpp seccomp.h
	$(CXX) -c $<

clean::
	rm -f *~ *.o
