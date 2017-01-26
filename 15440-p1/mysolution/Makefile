all: mylib.so server

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c -Iinclude

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl -Llib

clean:
	rm -f *.o *.so server

