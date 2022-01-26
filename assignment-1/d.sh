cd $1
mkdir ../files_mod
for file in *.txt; do
    nl -n'ln' $file | sed 's/[ \t]\+/,/g' > ../files_mod/$file
done