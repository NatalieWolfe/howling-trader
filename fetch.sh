#! /bin/bash

mkdir -p data/history/$1

for i in $(seq $4 $5); do
  file="data/history/$1/$2-$(printf "%02d" $3)-$(printf "%02d" $i).textproto"
  echo $1 $2-$3-$i
  bazel-bin/howling_tools/fetch \
    --flagfile=secrets.txt \
    --stock=$1 \
    --duration=12h \
    --start=$2-$3-${i}T08:30:00-05:00 \
    --minloglevel=3 \
    > $file
done

wc -l data/history/$1/*.textproto
