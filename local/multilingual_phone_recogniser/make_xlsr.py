import sys

import click
import numpy as np
import soundfile as sf
import torch

from s3prl.util.download import set_dir
from s3prl.nn import S3PRLUpstream
from kaldi_native_io import SequentialWaveReader, CompressedMatrixWriter, CompressionMethod


@click.command()
@click.option('--model_name', type=str, help='SSL model name')
@click.option('--input_rspecifier', type=str, help='Input rspecifier')
@click.option('--output_wspecifier', type=str, help='Output wspecifier')
@click.option('--layer', type=int, help='Output layer')
@click.option('--device', type=str, help='Device')
def main(model_name, input_rspecifier, output_wspecifier, layer, device):
  print(f'Using {device} with input {input_rspecifier} output {output_wspecifier}', file=sys.stderr)
  set_dir("./s3prl_download")
  model = S3PRLUpstream(model_name, "ckpts").to(device)
  model.eval()

  with SequentialWaveReader(input_rspecifier) as reader, \
      CompressedMatrixWriter(output_wspecifier) as writer:
    for name, wav in reader:
      wav = wav.data.numpy()[0]

      chunks = []
      chunk_length = 40
      print(f'Extracting feats for {name} with length {wav.shape[0] / 16000:.1f}s', file=sys.stderr)
      for start in range(0, wav.shape[0], chunk_length * 16000):
        with torch.no_grad():
          chunk = torch.FloatTensor(wav[start:start + chunk_length * 16000]).view(1, -1).to(device)
          chunk_len = torch.LongTensor([chunk.shape[-1]]).view(1, -1).to(device)
          chunks.append(model(chunk, chunk_len)[0][layer].cpu().numpy()[0])

      features = np.concatenate(chunks)
      writer.write(name, features, CompressionMethod.kAutomaticMethod)
      print(f'Extracted feats for {name} with shape {features.shape}', file=sys.stderr)

if __name__ == '__main__':
  main()
