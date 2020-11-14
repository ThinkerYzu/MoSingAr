TESTS := test_tinypack

all::

test:: $(TESTS)
	./test_tinypack

test_tinypack: test_tinypack.cpp tinypack.h
	$(CXX) -o $@ $<

clean:
	rm -f *~ $(TESTS)
