PROGRAM = qanddview

INCLUDEDIRS = \
	$(shell wx-config --cflags)\
	$(shell pkg-config gtk+-2.0 --cflags)

LIBDIRS = \
	-L/usr/X11R6/lib

LIBS = \
	$(shell wx-config --libs)

CXXSOURCES = ImagePanel.cpp  LinearImage.cpp  main.cpp  ScaledImageFactory.cpp imageresampler/resampler.cpp
CXXOBJECTS = $(CXXSOURCES:.cpp=.o)
CXXFLAGS = $(INCLUDEDIRS) -std=c++0x -Wall -Wextra
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
	$(RM) -f $(CXXOBJECTS) $(PROGRAM)
