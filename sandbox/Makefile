
all:: seccomp.o filter.o bridge.o

seccomp.o: seccomp.cpp seccomp.h bridge.h
	$(CXX) -c $<

filter.o: filter.cpp seccomp.h
	$(CXX) -c $<

bridge.o: bridge.cpp bridge.h
	$(CXX) -c $< -I../toolkits

clean::
	rm -f *~ *.o
