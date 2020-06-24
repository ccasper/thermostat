# This makefile is for x86-64 unit tests. Use the Arduino IDE for running/loading the program.
# To run unit tests, run "make clean && make test"
#
# Tests:
#  - calculate_iaq_score

GOOGLE_TEST_LIB = gtest
GOOGLE_TEST_INCLUDE = /usr/local/include

G++ = g++
G++_FLAGS = -c -Wall -I $(GOOGLE_TEST_INCLUDE)
LD_FLAGS = -L /usr/local/lib -l $(GOOGLE_TEST_LIB) -l pthread

OBJECTS = calculate_iaq_score_test.o
TARGET = calculate_iaq_score_test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	g++ -o $(TARGET) $(OBJECTS) $(LD_FLAGS)

%.o : %.cpp
	$(G++) $(G++_FLAGS) $<

test: all
	./calculate_iaq_score_test

clean:
	rm -f $(TARGET) $(OBJECTS)
                    
.PHONY: all clean
