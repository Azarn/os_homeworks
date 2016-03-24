CC=gcc
CFLAGS=-Wall
LDFLAGS=
SOURCES=cat.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=cat


all: $(SOURCES) $(EXECUTABLE)

clean:
	rm $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
