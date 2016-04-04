all: comm

LIB_INCLUDES=-L`pwd`/../
CFLAGS=-g -Wall
LIBS=-lsgsh_negotiate

comm: comm.c
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(LIBS) $^ -o $@ 

