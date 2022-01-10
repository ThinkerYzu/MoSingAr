CFLAGS := -I../toolkits -I../3rd-parties/sha2-1.0.1 -g

.PHONY: all clean test

all: libogl.a
	$(MAKE) -C tests

libogl.a: ogl.o oglproto.o
	$(AR) rc libogl.a ogl.o

ogl.o: ogl.cpp ogl.h otypes.h
	$(CXX) $(CFLAGS) -c $<

oglproto.o: oglproto.cpp oglproto.h ogl.h otypes.h ../mux/mux.h
	$(CXX) $(CFLAGS) -c $< -I../mux

clean:
	$(MAKE) -C tests clean
	rm -f *~ *.o
	rm -f *.a

test: all
	$(MAKE) -C tests test
