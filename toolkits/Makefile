# -fno-exceptions to avoid the symbol __gxx_personality_v0 of personality routine.
# see https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#base-personality
CFLAGS := -Wall -Werror -fpic -fvisibility=hidden -fno-builtin -nostdlib \
	-fno-builtin -fno-exceptions

TESTS := test_tinypack

OBJS := msghelper.o

all:: $(OBJS)

msghelper.o: msghelper.cpp msghelper.h
	$(CXX) $(CFLAGS) -c $<

test:: $(TESTS)
	./test_tinypack

test_tinypack: test_tinypack.cpp tinypack.h
	$(CXX) -o $@ $<

clean:
	rm -f *~ $(TESTS) $(OBJS)
