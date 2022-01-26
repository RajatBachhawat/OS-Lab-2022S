mkdir 1.b.files.out
for file in 1.b.files/*.txt; do
    sort -n $file > 1.b.files.out/${file#*/}
done
cat 1.b.files.out/* | sort -n | uniq -c | awk '{print $2"\t"$1}' > 1.b.files.out/1.b.out.txt