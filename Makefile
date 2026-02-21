CFLAGS = -O2 -Wall -pipe
CXXFLAGS += $(CFLAGS) -std=c++17

encode: btea.o encodetotext.o make_key.o process.o main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

testencode: btea.o encodetotext.o tests.o main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

-include Makefile.depend

.PHONY: clean depend
clean:
	rm -f encode testencode *.o
depend:
	for fname in *.c *.cpp; \
		do g++ -MM -MG $$fname; \
	done > Makefile.depend
