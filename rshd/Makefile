CC=g++
CXXFLAGS=-Wall -pedantic -std=c++11
LDFLAGS=
SOURCES=rshd.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=rshd


all: $(SOURCES) $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CXXFLAGS) -c $< -o $@
