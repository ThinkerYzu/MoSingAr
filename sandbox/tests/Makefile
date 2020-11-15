
all:: test_dup

test_dup: test_dup.cpp
	$(CXX) -o $@ $<

test: test_dup
	./test.sh ./test_dup

clean::
	rm -f *~ test_dup
