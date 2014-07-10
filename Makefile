CC = gcc

#IDIR = /usr/src/cfscc/src/

#CFLAGS = -fPIC -std=gnu99 -O2 -Wall -g -I$(IDIR)
#Full debug with -g3
CFLAGS = -fPIC -std=gnu99 -Wall -O2 #-I$(IDIR)
LDFLAGS = -pipe -Wall -lm -pthread -O2

EXEC = rtd_player

SRC = rtd_player.c rtd_player_helpers.c
OBJ = $(SRC:.c=.o)

HEADERS = defaults.h rtd_player_errors.h rtd_player_helpers.h rtd_player_struct.h $(EXEC).h

all: $(SRC) $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

.c.o: $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean

clean:
	rm -f *.o $(EXEC)
