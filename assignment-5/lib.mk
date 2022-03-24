libmemlab.a: memlab.o avl.o
	ar rcs libmemlab.a memlab.o avl.o
memlab.o: memlab.cpp memlab.h
	g++ -c memlab.cpp -lpthread
avl.o: avl.cpp avl.h
	g++ -c avl.cpp