#!/usr/bin/bash
for i in {0..99}; do
    ./$1 | grep "Here"
done