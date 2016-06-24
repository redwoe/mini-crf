#!/bin/bash

function generate-samples {
  echo "Generating samples"
  COUNT=10
  for (( i=0 ; i < $COUNT; i++)); do
    OUTPUT=$DIR/$MODE-$i.wav
    ORIGINAL=$DIR/original-$i.wav
    ./main-opt --mode $MODE --input $i \
      $EVAL \
      --path $DIR/path-$i.csv \
      --output $OUTPUT \
      --original $ORIGINAL \
      --cmp-csv $DIR/path-$i-cmp.csv \
      < $CONF_FILE
  done
}

function generate-comparisons {
  echo "Generating comparisons"
  for (( i=0 ; i < $COUNT; i++)); do
    INPUT=$DIR/$MODE-$i.wav
    ORIGINAL=$DIR/original-$i.wav
    OUTPUT=$DIR/comparisons-$i.csv
    ./main-opt --mode compare \
      --output $OUTPUT \
      --i $ORIGINAL --o $INPUT < $CONF_FILE
  done
}

function concatenate-comparisons {
  echo "Concatenating comparisons"
  CSV=$DIR/comparisons-all.csv
  head -n 1 $DIR/comparisons-0.csv > $CSV
  for (( i=0 ; i < $COUNT; i++)); do
    CMP=$DIR/comparisons-$i.csv
    tail -n $(expr `wc -l < $CMP` - 1) $CMP >> $CSV
  done

  CMP_TOTALS=$DIR/comparisons-totals.csv
  bash $(dirname $0)/extract-totals.sh $DIR $CMP_TOTALS
}

set -e
set -x

if [ $# -lt 2 ]; then
  echo "Usage: <cmd> <mode> <conf_file> [eval_corpus]"
  echo "Set SUFFIX=<smt> for an output dir suffix"
  exit 1
fi

MODE=$1
shift
if [ "$1" == "eval" ]; then
  EVAL="--eval"
  shift
fi

DIR=$1
CONF_FILE=$2

rm -rf $DIR
mkdir -p $DIR

generate-samples
generate-comparisons
concatenate-comparisons
