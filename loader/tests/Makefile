
BINS := hello test_execvpe

all:: $(BINS)

hello: hello.cpp
	$(CXX) -g -o $@ $<

test_execvpe: test_execvpe.cpp
	$(CXX) -g -o $@ $<

clean:
	rm -f *.o *~ $(BINS)
