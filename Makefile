all:
	gcc -fPIC -static -shared -o bindhack.so bindhack.c -lc -ldl
