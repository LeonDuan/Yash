all: build

build:
	gcc -o yash *.c

clean:
	rm -rf *.o yash
