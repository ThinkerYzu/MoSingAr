BINS := tryseccomp tryptrace fork_in_sig

all:: $(BINS)

tryseccomp: tryseccomp.cpp
	$(CXX) -o $@ tryseccomp.cpp -lpthread

tryptrace: tryptrace.cpp
	$(CXX) -o $@ tryptrace.cpp

fork_in_sig: fork_in_sig.cpp
	$(CXX) -o $@ fork_in_sig.cpp

clean::
	rm -f *.o $(BINS)
