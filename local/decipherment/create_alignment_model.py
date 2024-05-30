from __future__ import print_function

import click
import math

@click.command()
@click.option('--num_symbols', type=int, help='Number of symbols')
@click.option('--max_insertions', type=int, help='Maximum number of insertions')
@click.option('--prob_insertion', type=float, help='Probability of insertion')
@click.option('--max_deletions', type=int, help='Maximum number of deletions')
@click.option('--prob_deletion', type=float, help='Probability of deletion')
def create_alignment_model(num_symbols, max_insertions, prob_insertion, max_deletions, prob_deletion):
  del_symbol = num_symbols
  ins_states = [3 + x for x in range(max_insertions)]
  del_states = [3 + max_insertions + x for x in range(max_deletions)]

  # Special arcs to allow leading silence
  print(0, 1, 0, 0)
  print(0, 1, 1, 1)

  # Add arcs for substitution labels
  for symbol in range(2, num_symbols):
    print(1, 2, symbol, symbol, -math.log(1 - prob_insertion - prob_deletion))
    print(2, 2, symbol, symbol, -math.log(1 - prob_insertion - prob_deletion))
   
    for ins_state in ins_states:
      if ins_state != ins_states[-1]:
        print(ins_state, 2, symbol, symbol, -math.log(1 - prob_insertion))
      else:
        print(ins_state, 2, symbol, symbol, 0)

    for del_state in del_states:
      if del_state != del_states[-1]:
        print(del_state, 2, symbol, symbol, -math.log(1 - prob_deletion))
      else:
        print(del_state, 2, symbol, symbol, 0)

  # Add arcs for silence
  silence_symbol = 1
  for input_symbol in [0, silence_symbol]:
    print(2, 1, input_symbol, silence_symbol, 0)

    for ins_state in ins_states:
      if ins_state != ins_states[-1]:
        print(ins_state, 1, input_symbol, silence_symbol, 0)
      else:
        print(ins_state, 1, input_symbol, silence_symbol, 0)

    for del_state in del_states:
      if del_state != del_states[-1]:
        print(del_state, 1, input_symbol, silence_symbol, 0)
      else:
        print(del_state, 1, input_symbol, silence_symbol, 0)

  # Add arcs for deletions
  for state, next_state in zip([2] + del_states, del_states):
    print(state, next_state, del_symbol, 0, -math.log(prob_deletion))

  # Add arcs for insertions
  for state, next_state in zip([2] + ins_states, ins_states):
    for symbol in range(2, num_symbols):
      print(state, next_state, 0, symbol, -math.log(prob_insertion))

  # Add final states
  for state in [0, 1, 2] + ins_states + del_states:
    print(state)


if __name__ == '__main__':
  create_alignment_model()
