all:
	g++ -D GC -D GC_STACK_POP -I.. demofiles/demo1.cpp -L. -lmemlab -lpthread -o demo1
	g++ -D GC -D GC_STACK_POP -I.. demofiles/demo2.cpp -L. -lmemlab -lpthread -o demo2
	g++ -D GC -D GC_STACK_POP -I.. demofiles/demo3.cpp -L. -lmemlab -lpthread -o demo3
	g++ -D GC -D GC_STACK_POP -I.. demofiles/demo4.cpp -L. -lmemlab -lpthread -o demo4
	g++ -D GC -D GC_STACK_POP -I.. demofiles/demo5.cpp -L. -lmemlab -lpthread -o demo5