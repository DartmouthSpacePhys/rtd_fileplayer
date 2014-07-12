CC = gcc

#IDIR = /usr/src/cfscc/src/

CFLAGS = -fPIC -std=gnu99 -Wall -g #-O2 #-I$(IDIR)
LDFLAGS = -pipe -Wall -lm -pthread -g #-O2

EXEC = rtd_player

SRC = simple_fifo.c rtd_player.c rtd_player_helpers.c 
OBJ = $(SRC:.c=.o)

HEADERS = simple_fifo.h defaults.h rtd_player_errors.h rtd_player_helpers.h rtd_player_struct.h $(EXEC).h 

all: $(SRC) $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

.c.o: $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean

clean:
	rm -f *.o $(EXEC)
