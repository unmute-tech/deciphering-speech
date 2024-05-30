# -*- coding: utf-8 -*-

import re
import sys
import glob
import codecs
import unicodedata

import click
import tqdm
from multiprocessing import Pool
from collections import Counter

from nltk import word_tokenize

letter_repeated_three_times = re.compile(r'([^ ])\1\1')
word_repeated_three_times = re.compile(r'(\b[^ ]+\b )\1\1')
three_single_letter_words = re.compile(r'\b[^ ] [^ ] [^ ]\b')
is_punctuation_letter = re.compile(r'(COMMA|SEMICOLON|FULL STOP|EXCLAMATION MARK|QUESTION MARK)')

def load_lines(path):
  with open(path, 'r') as f:
    for line in f:
      yield line.strip()

class Normalizer:

  def __init__(self, script=None, allowed_characters=None):
    self.script = script
    self.allowed_characters = allowed_characters
    self.punctuation = {}

  def is_allowed(self, c):
    if self.allowed_characters:
      return c in self.allowed_characters

    try:
      name = unicodedata.name(c)
      return self.script in name and "DIGIT" not in name and "NUMBER" not in name and "PUNCTUATION" not in name
    except ValueError:
      return False
          
  def __call__(self, line):
    line = line.strip()
    line = unicodedata.normalize('NFC', line).lower()
  
    words = []
    unks = 0
    for word in word_tokenize(line):
      if all([not c.isalnum() for c in word]):
        continue
    
      if letter_repeated_three_times.search(word):
        words.append('<unk>')
        unks += 1
        continue
  
      if all([self.is_allowed(c) for c in word]):
        words.append(word)
      else:
        words.append('<unk>')
        unks += 1
  
    if len(words) < 5:
      return "", 0
  
    if float(unks) / len(words) >= 0.2:
      return "", 0
  
    normalized_line = u' '.join(words)
    if three_single_letter_words.search(normalized_line) is not None:
      return "", 0
  
    if word_repeated_three_times.search(normalized_line) is not None:
      return "", 0
  
    return normalized_line, len(words)

def estimate_allowed_characters(script, path):
  normalizer = Normalizer(script.upper())
  counter = Counter()
  total = 0
  target_characters = 1e6
  with tqdm.tqdm(desc='Selecting allowed characters', total=target_characters) as pbar:
    for sentence in load_lines(path):
      for c in normalizer(sentence)[0].replace('<unk>', ''):
        if c == ' ':
          continue

        counter[c] += 1
        total += 1
        pbar.update(1)

      if total >= target_characters:
        break

  return set([c for c, count in counter.most_common() if count / total >= 1e-4 and not is_punctuation(c)])

def is_punctuation(c):
  try:
    name = unicodedata.name(c)
  except ValueError:
    return True

  return is_punctuation_letter.search(name) is not None

@click.command()
@click.option('--script', type=str)
@click.option('--input-file', type=str)
@click.option('--output-file', type=str)
@click.option('--nj', type=int, default=4)
def main(script, input_file, output_file, nj):
  allowed_characters = estimate_allowed_characters(script, input_file)
  print('Allowed characters:', ''.join(sorted(allowed_characters)))

  total_words = 0
  target_words = 5e8
  with Pool(nj) as p, \
    tqdm.tqdm(desc='Normalizing text', total=target_words) as pbar, \
    open(output_file, 'w') as f:
      for line, num_words in p.imap(Normalizer(allowed_characters=allowed_characters), load_lines(input_file), chunksize=4096): 
        if num_words > 0:
          print(line, file=f)
          total_words += num_words
          pbar.update(num_words)
  
        if total_words >= target_words:
          break
 

if __name__ == '__main__':
  main()
