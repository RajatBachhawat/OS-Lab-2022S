all:
	g++ -I.. demofiles/demo1.cpp -L. -lmemlab -lpthread -o demo1
	g++ -I.. demofiles/demo2.cpp -L. -lmemlab -lpthread -o demo2
	g++ -I.. demofiles/demo3.cpp -L. -lmemlab -lpthread -o demo3
	g++ -I.. demofiles/demo4.cpp -L. -lmemlab -lpthread -o demo4
	g++ -I.. demofiles/demo5.cpp -L. -lmemlab -lpthread -o demo5