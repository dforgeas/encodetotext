CFLAGS = -O2 -Wall -pipe
CXXFLAGS = $(CFLAGS) -std=c++11

encode: btea.o encodetotext.o
	$(CXX) $(CXXFLAGS) -o $@ $^

testencode: CFLAGS += -DUNIT_TESTS
testencode: btea.o encodetotext.o
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f encode testencode *.o
