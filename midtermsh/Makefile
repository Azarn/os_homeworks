CC=g++
CXXFLAGS=-Wall -pedantic -std=c++11
LDFLAGS=
SOURCES=simplesh.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=simplesh


all: $(SOURCES) $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CXXFLAGS) -c $< -o $@
