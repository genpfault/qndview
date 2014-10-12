PROGRAM = qndview

INCLUDEDIRS = \
	$(shell wx-config --cflags)\
	$(shell pkg-config gtk+-2.0 --cflags)

LIBDIRS = \
	-L/usr/X11R6/lib

LIBS = \
	$(shell wx-config --libs)

CXXSOURCES = src/ImagePanel.cpp  src/main.cpp  src/ScaledImageFactory.cpp
CXXOBJECTS = $(CXXSOURCES:.cpp=.o)
CXXFLAGS = $(INCLUDEDIRS) -std=c++0x -Wall -Wextra -O3 -march=native -Iexternal
CXX = g++

LDFLAGS = $(LIBDIRS) $(LIBS)

all: $(PROGRAM)

$(PROGRAM): $(CXXOBJECTS)
	$(CXX) -o $@ $(CXXOBJECTS) $(LDFLAGS)

.depend:
	fastdep $(CXXSOURCES) > .depend

-include .depend

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) -f $(CXXOBJECTS) $(PROGRAM) .depend
