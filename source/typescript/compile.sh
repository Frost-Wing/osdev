rm -r typescript-source.c
ts2c index.ts
mv index.c typescript-source.c
python3 convert-frost.py