all:
	g++ demo1.cpp -L. -lmemlab -lpthread -o demo1
	g++ demo2.cpp -L. -lmemlab -lpthread -o demo2
