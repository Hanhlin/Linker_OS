linker: linker.cpp
	g++ -gdwarf-3 -std=c++11 linker.cpp -o linker
clean:
	rm -f linker *~