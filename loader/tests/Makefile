
BINS := hello

all:: $(BINS)

hello: hello.cpp
	$(CXX) -g -o $@ $<

clean:
	rm -f *.o *~ $(BINS)
