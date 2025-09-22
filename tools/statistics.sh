#!/bin/bash

#echo "d: static-initialized variable, e.g., static int i = 1;"
#echo "D: global-initialized variable, e.g., int j = 1"
#echo "b: static-uninitialized variable, e.g., static int i = 0; or static int i;"
#echo "B: global-uninitialized variable, e.g., int i = 0; or int i;"
#echo "r: static-constant data (BK platform uses this, CS uses t), e.g., static const mbedtls_cipher_info_t aes_128_ecb_info = {MBEDTLS_CIPHER_AES_128_ECB,MBEDTLS_MODE_ECB}"
#echo "R: global-constant data (BK platform uses this, CS uses t), e.g., const mbedtls_md_info_t mbedtls_md5_info {MBEDTLS_MD_MD5, 'MD5'}"
#echo "t: static-code segment, e.g., static void fun(void) {}"
#echo "T: global-code segment, e.g., void fun(void) {}"

get_statistics() {
    topic=$1
    grep -E "${topic}" symbols.all > symbols.${topic}

    grep -E " [tTdDrRWV] " symbols.${topic} > symbols.${topic}.rom
    awk 'BEGIN{size=0;}{size=size+$2;}END{printf "%-8d", size;}' symbols.${topic}.rom
    rm -rf symbols.${topic}.rom

    grep -E " [rR] " symbols.${topic} > symbols.${topic}.rom.readonly
    awk 'BEGIN{size=0;}{size=size+$2;}END{printf "%-8d", size;}' symbols.${topic}.rom.readonly
    rm -rf symbols.${topic}.rom.readonly

    grep -E " [bBdD] " symbols.${topic} > symbols.${topic}.ram
    awk 'BEGIN{size=0;}{size=size+$2;}END{printf "%-8d", size;}' symbols.${topic}.ram
    rm -rf symbols.${topic}.ram

    grep -E " [dD] " symbols.${topic} > symbols.${topic}.data
    awk 'BEGIN{size=0;}{size=size+$2;}END{printf "%-8d", size;}' symbols.${topic}.data
    rm -rf symbols.${topic}.data

    grep -E " [bB] " symbols.${topic} > symbols.${topic}.bss
    awk 'BEGIN{size=0;}{size=size+$2;}END{printf "%-8d", size;}' symbols.${topic}.bss
    rm -rf symbols.${topic}.bss
}

ELF_FILE_NAME=$1

if [ -z $ELF_FILE_NAME ]; then
	echo "no elf name"
	exit 1
fi

rm -rf symbols.*

nm -l -S -t d --size-sort $ELF_FILE_NAME > symbols.all
topics=(`find ./src -maxdepth 1 -type d ! -name 'src' -exec basename {} \; | sort`)
topics+=(`find ./board -maxdepth 1 -type d ! -name 'board' -exec basename {} \; | sort`)
topics+=(`find ./examples -maxdepth 1 -type d ! -name 'examples' -exec basename {} \; | sort`)
# topics=`grep -o '/src/\(.*\)/' symbols.all | cut -d/ -f3  | sort | uniq` 
topics+=("tuyaopen")

echo "resource statstics"
echo "rom-----ro------ram-----data----bss------name"
for topic in ${topics[@]}
do
get_statistics ${topic}
echo "${topic}"
done

rm -rf symbols.*


