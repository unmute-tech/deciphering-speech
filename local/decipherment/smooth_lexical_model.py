from __future__ import print_function

import sys
import click
import math
from collections import defaultdict

@click.command()
@click.option('--num_src_symbols', type=int, help='Number of source symbols')
@click.option('--num_tgt_symbols', type=int, help='Number of target symbols')
@click.option('--alpha', type=float, help='Alpha')
def create_alignment_model(num_src_symbols, num_tgt_symbols, alpha):
  weights = defaultdict(lambda: defaultdict(lambda: (1 - alpha) / (num_src_symbols - 1)))

  for line in sys.stdin:
    if len(line.strip().split()) == 4:
      line += " 0"

    if len(line.strip().split()) != 5:
      continue

    _, _, src_symbol, tgt_symbol, weight = line.strip().split()
    weights[int(src_symbol)][int(tgt_symbol)] += alpha * math.exp(-float(weight))

  for src_symbol in range(2, num_src_symbols):
    for tgt_symbol in range(2, num_tgt_symbols + 1):
      log_prob = -math.log(weights[src_symbol][tgt_symbol])
      print(0, 0, src_symbol, tgt_symbol, log_prob)

  print(0, 0, 1, 1)
  print(0)


if __name__ == '__main__':
  create_alignment_model()
