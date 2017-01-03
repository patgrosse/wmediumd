CFLAGS = -g -Wall -O2
LDFLAGS =

OBJECTS=client.o ../wmediumd/wserver_messages.o

all: client

client: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

clean:
	rm -f $(OBJECTS) client
