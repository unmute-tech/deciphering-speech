from __future__ import print_function

import click
import math
import random

@click.command()
@click.option('--num_src_symbols', type=int, help='Number of source symbols')
@click.option('--num_tgt_symbols', type=int, help='Number of target symbols')
@click.option('--random_weights', type=bool, default=False, help='Random weights?')
@click.option('--seed', type=float, default=0, help='Random seed')
def create_alignment_model(num_src_symbols, num_tgt_symbols, random_weights, seed):
  random.seed(seed)
  for tgt_symbol in range(2, num_tgt_symbols + 1):
    probs = [random.random() for _ in range(2, num_src_symbols)]
    log_probs = [-math.log(p / float(sum(probs))) for p in probs]

    for src_symbol in range(2, num_src_symbols):
      if random_weights:
        log_prob = log_probs[src_symbol - 2]
      else:
        log_prob = -math.log(1. / (num_src_symbols - 2))

      print(0, 0, src_symbol, tgt_symbol, log_prob)

  print(0, 0, 1, 1)
  print(0)


if __name__ == '__main__':
  create_alignment_model()
