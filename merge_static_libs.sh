#!/bin/bash

TMPDIR="merge_static_library_tmp_dir"
TARGET=$1
IFS=" "
LIBS=($@)
unset LIBS[0]

#create tmp dir
mkdir -p $TMPDIR
cd $TMPDIR

#unpack all static librarys
for lib in ${LIBS[@]}
do
    ar x $lib
    # echo "unpacking $lib"
done

#create new static linked library
ar cr $TARGET *.o
echo "target $TARGET created"

#delete tmp dir
cd ..
rm -r $TMPDIR
