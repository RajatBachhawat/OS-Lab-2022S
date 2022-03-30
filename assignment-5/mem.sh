#!/usr/bin/bash
> $2
> $3
for i in {0..99}; do
    ./$1 | grep "+ Amt" > tmp.txt
    cut -d' ' -f10 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> $2
    cut -d' ' -f14 tmp.txt | tr '\n' ',' | sed "s/,$//g" >> $3
    echo >> $2
    echo >> $3
done