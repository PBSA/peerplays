#! /bin/bash

cd $1/libraries/fc_pp

if [[ -d include/fc_pp ]]; then
    exit 0
fi

echo "Rename PearPlays fc to fc_pp"

git status > old_git_status.txt 2>/dev/null

# Instructions to `sed` match to this patterns:
# 1. fc::method
# 2. #include <fc/...>
# 3. namespace fc{
# 4. namespace fc
# 5. using namespace fc;
# 6. fc_swap
for file in $(find . -type f); do
    if [[ $file =~ .*vendor/.* ]]; then
        continue
    fi
    sed -e 's!fc::!fc_pp::!g'       \
        -e 's!fc/!fc_pp/!g'         \
        -e 's!fc[ ]*{!fc_pp {!g'    \
        -e 's!fc[ ]*$!fc_pp!g'      \
        -e 's!fc;$!fc_pp;!g'        \
        -e 's!fc_swap!fc_pp_swap!g' \
        $file > temp_file && mv temp_file $file
done

mv include/fc include/fc_pp 2>/dev/null

# Run this to clear changes (if you use git repo for fc)
# `cd libraries/fc_pp/ && git reset --hard HEAD && rm -rf include/fc_pp && cd -`