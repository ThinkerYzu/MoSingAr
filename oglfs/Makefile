CFLAGS := -I../toolkits -I../3rd-parties/sha2-1.0.1 -g

.PHONY: all clean test

all: libogl.a
	$(MAKE) -C tests

libogl.a: ogl.o
	$(AR) rc libogl.a ogl.o

ogl.o: ogl.cpp ogl.h otypes.h
	$(CXX) $(CFLAGS) -c $<

clean:
	$(MAKE) -C tests clean
	rm -f *~ *.o
	rm -f *.a

test: all
	$(MAKE) -C tests test
