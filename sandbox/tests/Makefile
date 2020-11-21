CFLAGS := -g

all:: test_dup tdtrampoline test_trampoline

test_dup: test_dup.cpp
	$(CXX) $(CFLAGS) -o $@ $<

test_trampoline: test_trampoline.cpp
	$(CXX) $(CFLAGS) -o $@ $<

tdtrampoline: faketrampoline.cpp
	$(CXX) $(CFLAGS) -o $@ $<

test: test_dup test_trampoline
	./test.sh ./test_dup

clean::
	rm -f *~ test_dup test_trampoline tdtrampoline
