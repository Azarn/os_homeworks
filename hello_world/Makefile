CC=gcc
CFLAGS=-Wall
LDFLAGS=
SOURCES=hello_world.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=hello_world


all: $(SOURCES) $(EXECUTABLE)

clean:
	rm $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
