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
	$(CXX) -DDUMMY -DTEST_CC_CHANNEL -o $@ $< ../scout.cpp -I.. -I../../loader -I../../toolkits ../../loader/libloader.so

fake_cc: fake_cc.cpp
	$(CXX) -o $@ $<

test: test_dup test_trampoline test_scout_conn fake_cc
	./test.sh ./test_dup
	@echo
	./fake_cc ./test_scout_conn
	@echo
	./fake_cc ./test_trampoline

clean::
	rm -f *~ $(BINS)
