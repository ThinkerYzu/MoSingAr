
all:: tryseccomp tryptrace

tryseccomp: tryseccomp.cpp
	$(CXX) -o $@ tryseccomp.cpp -lpthread

tryptrace: tryptrace.cpp
	$(CXX) -o $@ tryptrace.cpp
