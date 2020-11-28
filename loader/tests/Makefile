
BINS := test_scout_conn

all:: $(BINS)

test_scout_conn: test_scout_conn.cpp ../cmdcenter.h ../scout.h
	$(CXX) -o $@ $< -I.. ../libloader.so

clean:
	rm -f *.o *~ $(BINS)
