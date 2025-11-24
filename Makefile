simulator: main.cpp
	g++ -std=c++11 -Wno-deprecated-declarations main.cpp -o simulator

simulator2: main2.cpp
	g++ -std=c++11 -Wno-deprecated-declarations main2.cpp -o simulator2

clean:	
	rm -f simulator simulator2