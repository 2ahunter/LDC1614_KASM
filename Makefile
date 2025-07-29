objects = ldc1614.o main.o UDP_client.o

CFLAGS = -Wall -Wextra -pedantic -std=gnu17

LDLIBS = -lwiringPi -lc


# $@ is the target, $^ are the prerequisites
ldc_test: $(objects)
	cc -o $@ $^ $(LDLIBS)

ldc_it_test: ldc_it_test.c ldc1614.o
	cc -o $@ ldc_it_test.c ldc1614.o $(LDLIBS)

main.o: main.c UDP_client.o

UDP_client.o: UDP_client.c UDP_client.h

.PHONY : clean
clean :
	rm ldc_test $(objects)