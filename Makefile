CXX=clang++
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
	$(CXX) $(CXXFLAGS) -MM -MP -o $@ $<

-include $(DEPS)
