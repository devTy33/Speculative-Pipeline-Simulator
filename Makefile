pipesim: pipesim.cpp
	g++ -std=c++11 -Wno-deprecated-declarations pipesim.cpp -o pipesim

run: pipesim
	./pipesim < trace2.dat

clean:	
	rm -f pipesim