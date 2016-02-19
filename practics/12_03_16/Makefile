CC=gcc
CFLAGS=-Wall
LDFLAGS=
SOURCES=cat_grep.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=cat_grep


all: $(SOURCES) $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
