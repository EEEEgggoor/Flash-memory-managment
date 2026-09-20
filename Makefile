CFLAGS = -I.

main: main.c
	gcc $(CFLAGS) -o main main.c