simulator: main.cpp
	g++ -std=c++11 -Wno-deprecated-declarations main.cpp -o simulator

clean:	
	rm -f simulator