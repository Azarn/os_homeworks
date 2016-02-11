all: hello

clean:
	rm hello.o hello

hello: hello.o
	gcc -s hello.o -o hello

hello.o: hello.c
	gcc -c hello.c -o hello.o
