CFLAGS := -g

BINS := test_dup tdtrampoline test_trampoline test_scout_conn fake_cc

all:: $(BINS)

test_dup: test_dup.cpp
	$(CXX) $(CFLAGS) -o $@ $<

test_trampoline: test_trampoline.cpp
	$(CXX) $(CFLAGS) -o $@ $<

tdtrampoline: faketrampoline.cpp
	$(CXX) $(CFLAGS) -o $@ $<

test_scout_conn: test_scout_conn.cpp ../../loader/cmdcenter.h ../scout.h
	$(CXX) -DDUMMY -DTEST_CC_CHANNEL -o $@ $< ../scout.cpp -I.. -I../../loader ../../loader/libloader.so

fake_cc: fake_cc.cpp
	$(CXX) -o $@ $<

test: test_dup test_trampoline
	./test.sh ./test_dup

clean::
	rm -f *~ $(BINS)
