CXX=clang++
CPPFLAGS=-I.
CXXFLAGS=-std=c++11 -g -O2 -Wall -Wextra
LDFLAGS=-flto
LINK.o=$(CXX)

SOURCES=$(wildcard *.cpp)
OBJS=$(SOURCES:.cpp=.o)
DEPS=$(SOURCES:.cpp=.d)

.PHONY: all clean

all: test

clean:
	$(RM) *.d *.o

test: $(OBJS)

%.d: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM -MP -o $@ $<

-include $(DEPS)
