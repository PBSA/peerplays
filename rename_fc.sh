#! /bin/bash

cd $1/libraries/fc_pp

if [[ -d include/fc_pp ]]; then
    exit 0
fi

echo "Rename PearPlays fc to fc_pp"

git status > old_git_status.txt 2>/dev/null

FILE_ARRAY=( ./src/compress/zlib.cpp ./tests/compress/compress.cpp ./src/compress/miniz.c )

METHODS_ARRAY=(
    mz_adler32 mz_crc32 mz_free mz_version mz_deflateInit mz_deflateInit2 tdefl_create_comp_flags_from_zip_params \
    tdefl_init mz_deflateEnd mz_deflateReset mz_deflate tdefl_compress tdefl_get_adler32 mz_deflateBound \
    mz_compress2 mz_compress mz_compressBound mz_inflateInit2 mz_inflateInit mz_inflate tinfl_decompress mz_inflateEnd \
    mz_uncompress mz_error tinfl_decompress_mem_to_heap tinfl_decompress_mem_to_mem tinfl_decompress_mem_to_callback \
    tdefl_compress_buffer tdefl_get_prev_return_status tdefl_compress_mem_to_output tdefl_compress_mem_to_heap tdefl_compress_mem_to_mem \
    tdefl_write_image_to_png_file_in_memory_ex tdefl_write_image_to_png_file_in_memory mz_zip_reader_init mz_zip_reader_end \
    mz_zip_reader_init_mem mz_zip_reader_init_file mz_zip_reader_get_num_files mz_zip_reader_is_file_encrypted \
    mz_zip_reader_is_file_a_directory mz_zip_reader_file_stat mz_zip_reader_get_filename mz_zip_reader_locate_file\
    mz_zip_reader_extract_to_mem_no_alloc mz_zip_reader_extract_file_to_mem_no_alloc mz_zip_reader_extract_to_mem \
    mz_zip_reader_extract_file_to_mem mz_zip_reader_extract_to_heap mz_zip_reader_extract_to_callback mz_zip_reader_extract_file_to_callback \
    mz_zip_reader_extract_to_file mz_zip_writer_init mz_zip_writer_init_heap mz_zip_writer_end mz_zip_writer_init_file \
    mz_zip_writer_init_from_reader mz_zip_writer_add_mem mz_zip_writer_add_mem_ex mz_zip_writer_add_file mz_zip_writer_add_from_zip_reader \
    mz_zip_writer_finalize_archive mz_zip_add_mem_to_archive_file_in_place mz_zip_extract_archive_file_to_heap \
    mz_zip_reader_extract_file_to_heap mz_zip_reader_extract_file_to_file mz_zip_writer_finalize_heap_archive
) 

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

for file in "${FILE_ARRAY[@]}"; do
    for method in "${METHODS_ARRAY[@]}"; do
        sed -e "s|\b$method\b|pp_$method|g" \
            $file > temp_file && mv temp_file $file
    done
done

mv include/fc include/fc_pp 2>/dev/null

# Run this to clear changes (if you use git repo for fc)
# `cd libraries/fc_pp/ && git reset --hard HEAD && rm -rf include/fc_pp && cd -`