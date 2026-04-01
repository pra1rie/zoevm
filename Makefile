
CFLAGS = -Isauce -Wall -Werror

all:
	$(CC) -O2 sauce/main.c -o zoe $(CFLAGS)

debug:
	$(CC) sauce/main.c -o zoe $(CFLAGS) -ggdb -D_DEBUG

