CC=gcc
CFLAGS=-Wall
LDFLAGS=
SOURCES=sigusr.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=sigusr


all: $(SOURCES) $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
