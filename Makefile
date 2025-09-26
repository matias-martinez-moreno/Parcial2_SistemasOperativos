CFLAGS = -O2 -Wall -Wextra -pthread
LDFLAGS = -pthread

INCS = -Iinclude

SRV_OBJS = src/server.o src/rooms.o src/commands.o src/persistence.o src/util.o
CLI_OBJS = client/client.o src/util.o

all: server client_app

server: $(SRV_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client_app: $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

clean:
	rm -f server client_app $(SRV_OBJS) $(CLI_OBJS)
