from __future__ import print_function

import sys
import click
import math
from collections import defaultdict

@click.command()
@click.option('--num_src_symbols', type=int, help='Number of source symbols')
@click.option('--num_tgt_symbols', type=int, help='Number of target symbols')
@click.option('--keep_top_tgt_symbols', type=int, help='Number of target symbols to keep for each source symbol')
def create_alignment_model(num_src_symbols, num_tgt_symbols, keep_top_tgt_symbols=5):
  weights_per_src_symbols = defaultdict(dict)
  weights_per_tgt_symbols = defaultdict(dict)

  for line in sys.stdin:
    if len(line.strip().split()) != 5:
      continue

    _, _, src_symbol, tgt_symbol, weight = line.strip().split()
    weight = math.exp(-float(weight))
    weights_per_src_symbols[int(src_symbol)][int(tgt_symbol)] = weight
    weights_per_tgt_symbols[int(tgt_symbol)][int(src_symbol)] = weight

  src_thresholds = defaultdict(float)
  for src_symbol in range(2, num_src_symbols):
    if src_symbol not in weights_per_src_symbols:
      continue

    src_thresholds[src_symbol] = sorted(weights_per_src_symbols[src_symbol].values())[-keep_top_tgt_symbols]

  tgt_thresholds = defaultdict(float)
  tgt_thresholds[num_tgt_symbols] = 0
  for tgt_symbol in range(2, num_tgt_symbols):
    if tgt_symbol not in weights_per_tgt_symbols:
      continue

    tgt_thresholds[tgt_symbol] = sorted(weights_per_tgt_symbols[tgt_symbol].values())[-keep_top_tgt_symbols]

  prob_sum = defaultdict(float)
  for src_symbol in range(2, num_src_symbols):
    if src_symbol not in weights_per_src_symbols:
      continue

    for tgt_symbol in range(2, num_tgt_symbols + 1):
      if tgt_symbol not in weights_per_src_symbols[src_symbol]:
        continue

      weight = weights_per_src_symbols[src_symbol][tgt_symbol]
      if weight >= src_thresholds[src_symbol] or weight >= tgt_thresholds[tgt_symbol]:
        prob_sum[tgt_symbol] += weights_per_src_symbols[src_symbol][tgt_symbol]

  for src_symbol in range(2, num_src_symbols):
    if src_symbol not in weights_per_src_symbols:
      continue

    threshold = sorted(weights_per_src_symbols[src_symbol].values())[-keep_top_tgt_symbols]
    for tgt_symbol in range(2, num_tgt_symbols + 1):
      if tgt_symbol not in weights_per_src_symbols[src_symbol]:
        continue

      weight = weights_per_src_symbols[src_symbol][tgt_symbol]
      if weight >= src_thresholds[src_symbol] or weight >= tgt_thresholds[tgt_symbol]:
        log_prob = -math.log(weight / prob_sum[tgt_symbol])
        print(0, 0, src_symbol, tgt_symbol, log_prob)

  print(0, 0, 1, 1)
  print(0)


if __name__ == '__main__':
  create_alignment_model()
