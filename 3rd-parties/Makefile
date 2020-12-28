LIBS := libsha2.a

.PHONY: all clean test

all: $(LIBS)

libsha2.a: sha2-1.0.1/sha2.o
	$(AR) rc libsha2.a sha2-1.0.1/sha2.o

sha2-1.0.1/sha2.o: sha2-1.0.1/sha2.c
	$(CC) -c -o $@ -Isha2-1.0.1/ sha2-1.0.1/sha2.c

clean:
	rm -f $(LIBS) sha2-1.0.1/*.o;
	rm -f *~

test:
