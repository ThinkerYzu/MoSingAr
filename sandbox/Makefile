
CFLAGS := -Wall -Werror -fpic

all:: libtongdao.so

libtongdao.so: seccomp.o filter.o bridge.o bootstrap.o
	$(CXX) -shared -o $@ seccomp.o filter.o bridge.o bootstrap.o

seccomp.o: seccomp.cpp seccomp.h bridge.h
	$(CXX) $(CFLAGS) -c $<

filter.o: filter.cpp seccomp.h
	$(CXX) $(CFLAGS) -c $<

bridge.o: bridge.cpp bridge.h
	$(CXX) $(CFLAGS) -c $< -I../toolkits

bootstrap.o: bootstrap.cpp
	$(CXX) $(CFLAGS) -c $<

clean::
	rm -f *~ *.o libtongdao.so
