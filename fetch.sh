for i in {1..31}; do
  file="data/history/NVDA/2025-10-$(printf "%02d" $i).textproto"
  bazel-bin/howling_tools/fetch \
    --flagfile=secrets.txt \
    --stock=NVDA \
    --duration=12h \
    --start=2025-10-${i}T08:30:00-05:00 \
    --minloglevel=3 \
    > $file
  wc -l $file
done
