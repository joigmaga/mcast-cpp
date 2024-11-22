#
#  Standard g++ compile
#
CXX           := g++
CXXFLAGS      := -I ./include -std=c++11
CXXEXTRAFLAGS := -Wall -Werror

# what to do
SOURCES  := mcast.cpp address.cpp logging.cpp 
OBJECTS  := ${SOURCES:.cpp=.o} 
PROGRAMS := ${SOURCES:.cpp=}

# Programs linking multiple objects 
test_address: test_address.o address.o logging.o
	${CXX} $^ -o $@

#
#  Putting everything together 
#
.PHONY: all
all: ${PROGRAMS}

${PROGRAMS} : % : %.o Makefile
	${CXX} $< -o $@

clean-objects:
	rm -f ${OBJECTS}

clean:
	rm -f ${PROGRAMS} ${OBJECTS}

%.o: %.cpp Makefile
	${CXX} ${CXXFLAGS} -c $<

