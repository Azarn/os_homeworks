CC=g++
CXXFLAGS=-Wall -pedantic -std=c++11
LDFLAGS=
SOURCES=midterm.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=midterm


all: $(SOURCES) $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CXXFLAGS) -c $< -o $@
