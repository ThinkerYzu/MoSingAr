
BINS := test_mux

all:: $(BINS)

clean::
	rm -f $(BINS) *~

test_mux: mux.cpp mux.h
	$(CXX) -DTEST -g -o $@ mux.cpp

test:: test_mux
	./test_mux
