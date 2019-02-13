CFLAGS = -O2 -Wall -pipe
CXXFLAGS += $(CFLAGS)

encode: btea.o encodetotext.o process.o main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

testencode: btea.o encodetotext.o tests.o main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

encodetotext.o: encodetotext.hpp btea.h
process.o: encodetotext.hpp
tests.o: encodetotext.hpp

.PHONY: clean
clean:
	rm -f encode testencode *.o
