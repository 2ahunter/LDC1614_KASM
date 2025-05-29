objects = ldc1614.o main.o

CFLAGS = -Wall -Wextra -pedantic -std=gnu17

LDLIBS = -lwiringPi


ldc_test: $(objects)
	cc -o $@ $^ $(LDLIBS) # $@ is the target, $^ are the prerequisites


	
.PHONY : clean
clean :
	rm ldc_test $(objects)