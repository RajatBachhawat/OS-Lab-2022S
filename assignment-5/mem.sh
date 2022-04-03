#!/usr/bin/bash
> t11.csv
> t12.csv
> t21.csv
> t22.csv
> t31.csv
> t32.csv
g++ -I.. demofiles/$1.cpp -L. -lmemlab -lpthread -o $1
for i in {0..99}; do
    ./$1 ${@:2} | grep "+ Amt" > tmp.txt
    cut -d' ' -f10 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> t11.csv
    cut -d' ' -f14 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> t12.csv
    echo >> t11.csv
    echo >> t12.csv
done
g++ -D GC -I.. demofiles/$1.cpp -L. -lmemlab -lpthread -o $1
for i in {0..99}; do
    ./$1 ${@:2} | grep "+ Amt" > tmp.txt
    cut -d' ' -f10 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> t21.csv
    cut -d' ' -f14 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> t22.csv
    echo >> t21.csv
    echo >> t22.csv
done
g++ -D GC -D GC_STACK_POP -I.. demofiles/$1.cpp -L. -lmemlab -lpthread -o $1
for i in {0..99}; do
    ./$1 ${@:2} | grep "+ Amt" > tmp.txt
    cut -d' ' -f10 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> t31.csv
    cut -d' ' -f14 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> t32.csv
    echo >> t31.csv
    echo >> t32.csv
done