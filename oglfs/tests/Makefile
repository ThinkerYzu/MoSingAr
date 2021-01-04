TESTS := basic_objects
CFLAGS := -I../ -g

.PHONY: all test clean

all: $(TESTS)

test: $(TESTS)
	rm -rf test_repo symlink-test
	./basic_objects
	rm -rf test_repo symlink-test

basic_objects: basic_objects.cpp ../libogl.a
	$(CXX) $(CFLAGS) -o $@ $< ../libogl.a ../../3rd-parties/libsha2.a

clean:
	rm -f *.o *~ $(TESTS) test_repo
