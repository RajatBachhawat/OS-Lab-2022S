cd $1
find -name '*.*' -type f | sed 's/.*\.//g' | sort | uniq | xargs -I%_ sh -c 'mkdir %_;find -name "*.%_" -type f -exec mv -nt %_ {} +;'
mkdir Nil
find ! -name "*.*" -type f -exec mv -nt "Nil" {} +;rm -rf Num_*