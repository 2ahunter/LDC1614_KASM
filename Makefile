objects = ldc1614.o main.o UDP_client.o

CFLAGS = -Wall -Wextra -pedantic -std=gnu17

LDLIBS = -lwiringPi -lc


# $@ is the target, $^ are the prerequisites
ldc_test: $(objects)
	cc -o $@ $^ $(LDLIBS)

main.o: main.c UDP_client.o

UDP_client.o: UDP_client.c UDP_client.h

.PHONY : clean
clean :
	rm ldc_test $(objects)