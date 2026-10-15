CXX ?= c++
AR ?= ar
CPPFLAGS ?=
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic

SOURCES := file_manager.cpp protocol.cpp stream_decoder.cpp
OBJECTS := $(SOURCES:.cpp=.o)
LIBRARY := libcnt4007.a

.PHONY: all check clean

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

check:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fsyntax-only $(SOURCES)

clean:
	rm -f $(OBJECTS) $(LIBRARY)