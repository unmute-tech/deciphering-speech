from __future__ import print_function

import io
import sys
import click
import unicodedata
import codecs

def letter_name(l):
  return unicodedata.name(l).replace(' ', '-')

@click.command()
@click.option('--path', type=str, help='Path to the lexicon')
@click.option('--sil-phone', type=str, default='SIL', help='sil phone')
@click.option('--output-leading-silence', type=bool, default=False, help='output leading silence')
def apply_lexicon(path, sil_phone, output_leading_silence):
  if path is not None:
    lexicon = {}
    with codecs.open(path, 'r', 'utf-8') as f:
      for line in f:
        word, _, pronunciation = line.strip().split(None, 2)
        word = unicodedata.normalize('NFC', word)
        lexicon[word] = pronunciation
    
    for line in io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8'):
      if " " not in line.strip():
        continue

      utt, words = line.strip().split(None, 1)
      words = [unicodedata.normalize('NFC', w) for w in words.split() if w in lexicon]
      if len(words) > 0:
        print(utt, (" %s " % sil_phone).join(lexicon[word] for word in words), sil_phone)

      if output_leading_silence:
        print(utt, sil_phone, (" %s " % sil_phone).join(lexicon[word] for word in words), sil_phone)
  else:
    for line in sys.stdin:
      utt, words = line.strip().split(None, 1)
      pronunciation = (" %s " % sil_phone).join(" ".join(letter_name(l) for l in word) for word in words.split())
      if output_leading_silence:
        print(utt, sil_phone, pronunciation, sil_phone)
      else:
        print(utt, pronunciation, sil_phone)

if __name__ == '__main__':
  apply_lexicon()
