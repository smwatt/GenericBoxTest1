#!/bin/bash

OUTDIR=tag-test-results

echo "Removing any old results"
rm -rf "$OUTDIR" "$OUTDIR.tgz" temp
mkdir "$OUTDIR"

for olevel in 0 1 2 3 
do
    echo "Compiling optimization level $olevel"
    g++ --std=c++20 -O$olevel --pedantic trial_generic.cpp -o trial_generic_$olevel
    g++ -S --std=c++20 -O$olevel --pedantic trial_generic.cpp 
    mv trial_generic.s "$OUTDIR/trial_generic_$olevel.s"
done

for size in 1000 10000 100000 1000000 10000000 100000000
do
    for olevel in 0 1 2 3 
    do
        echo "========= -O$olevel --time $size ========="
        "./trial_generic_$olevel" --time $size | tee temp
        (echo "========= -O$olevel --time $size =========" ; cat temp) >> \
            "$OUTDIR"/trial_generic_$olevel.log
        rm temp
    done
done
mv trial_generic_[0123] "$OUTDIR"

g++ -v > "$OUTDIR/g++-v.out"

LSCPU="`which lscpu`"
if [ -n "$LSCPU" ] ; then
    "$LSCPU" > "$OUTDIR/lscpu.out"
fi

UNAME="`which uname`"
if [ -n "$UNAME" ] ; then
    "$UNAME" > "$OUTDIR/uname.out"
fi


tar -zcvf tag-test-results.tgz "$OUTDIR"
