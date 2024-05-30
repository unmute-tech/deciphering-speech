#!/bin/bash

lang=$1

sed -n 's/_[BIES] .*//p;s/<eps> 0/<eps>/p' $lang/phones.txt | \
  uniq | \
  awk '{print $1, NR - 1}' > $lang/phones_pure.txt

awk '
  BEGIN {
    split(";_B;_I;_E;_S;", word_positions, ";")
  }

  {
    for (j = 1; j <= 5; j++) {
      printf("%s%s %d\n", $1, word_positions[j], $2);
    }
  }' $lang/phones_pure.txt | \
  sort | \
  join <(sort $lang/phones.txt) - | \
  awk '{print $1, $3}' | \
  sort -k 2,2n > $lang/phones_to_pure_id.txt

awk '
  BEGIN {
    split(";_B;_I;_E;_S;", word_positions, ";")
  }

  {
    for (j = 1; j <= 5; j++) {
      printf("%s%s %d\n", $1, word_positions[j], $2);
    }
  }' $lang/phones_pure.txt | \
  sort | \
  join <(sort $lang/phones.txt) - | \
  awk '{print $2, $3}' | \
  sort -k 2,2n > $lang/phones_to_pure_id_map.int
