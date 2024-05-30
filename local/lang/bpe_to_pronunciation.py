import sys
import codecs
import unicodedata

def letter_name(l):
  return unicodedata.name(l).replace(' ', '-')

with codecs.open(sys.argv[1], 'r', 'utf-8') as f_in, codecs.open(sys.argv[2], 'w', 'utf-8') as f_out:
  for line in f_in:
    word = line.strip()
    print(word, " ".join([letter_name(l) for l in word.strip('@')]), file=f_out)
