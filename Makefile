#
#  Standard g++ compile
#
CXX           := g++
CXXFLAGS      := -I ./include -std=c++11
CXXEXTRAFLAGS := -Wall -Werror

# what to do
PROGRAMS        := test_address test_getifaddrs
SOURCES	        := address.cpp logging.cpp getifaddrs.cpp
OBJECTS	        := ${SOURCES:.cpp=.o} 
PROGRAM_OBJECTS := ${PROGRAMS:=.o}

#
#  Putting everything together 
#
.PHONY: all

all: ${PROGRAMS}

${PROGRAMS}: % : %.o ${OBJECTS} Makefile
	${CXX} $< ${OBJECTS} -o $@

# add extra programs here
#
test2: test2.o logging.o
	${CXX} $^ -o $@

%.o: %.cpp Makefile
	${CXX} ${CXXFLAGS} ${CXXEXTRAFLAGS} -c $<

clean-objects:
	rm -f ${PROGRAM_OBJECTS} ${OBJECTS}

clean:
	rm -f ${PROGRAMS} ${PROGRAM_OBJECTS} ${OBJECTS}

