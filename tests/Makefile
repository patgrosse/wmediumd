CFLAGS = -g -Wall -O2
LDFLAGS =

OBJECTS=client.o ../wmediumd/wserver_messages.o ../wmediumd/wserver_messages_network.o

all: client

client: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

clean:
	rm -f $(OBJECTS) client
