CXX=clang++
CPPFLAGS=-I.
CXXFLAGS=-std=c++11 -g -O2 -Wall -Wextra
LDFLAGS=-flto
LDADD=-lvpx
V=0

SOURCES=$(wildcard *.cpp)
OBJS=$(SOURCES:.cpp=.o)
DEPS=$(SOURCES:.cpp=.d)

ifeq ($(V),1)
	SILENT=@\#
	VERBOSE=
else
	SILENT=@printf " %-5s $@\n"
	VERBOSE=@
endif

.PHONY: all clean

all: test

clean:
	$(RM) *.d *.o

test: $(OBJS)

%.d: %.cpp
	$(SILENT) DEP
	$(VERBOSE)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM -MP -o $@ $<

%.o: %.cpp
	$(SILENT) CXX
	$(VERBOSE)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

%: %.o
	$(SILENT) CXXLD
	$(VERBOSE)$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDADD)

-include $(DEPS)
