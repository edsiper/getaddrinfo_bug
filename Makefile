all:
	gcc -g -Wall main.c -o test_getaddrinfo

clean:
	rm -rf *.o *.o~ test_getaddrinfo
