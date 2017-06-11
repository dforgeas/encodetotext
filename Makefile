CFLAGS = -O2 -Wall
CXXFLAGS = $(CFLAGS) -std=c++11

encode: btea.o encodetotext.o
	g++ $(CXXFLAGS) -o $@ $^

testencode: CFLAGS += -DUNIT_TESTS
testencode: btea.o encodetotext.o
	g++ $(CXXFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f encode testencode *.o
