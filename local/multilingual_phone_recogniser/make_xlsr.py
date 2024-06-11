import io
import glob
import numpy as np
import tarfile
import soundfile as sf
import torch

import sys
import os
import click

from s3prl.util.download import set_dir
from s3prl.nn import S3PRLUpstream

from kaldi_native_io import SequentialWaveReader, CompressedMatrixWriter, CompressionMethod
from multiprocessing import Process, JoinableQueue

def writer_process(q, output_wspecifier):
    with CompressedMatrixWriter(output_wspecifier) as writer:
        while True:
            val = q.get()
            if val is None:
                break

            name, features = val
            writer.write(name, features, CompressionMethod.kAutomaticMethod)
            q.task_done()

        q.task_done()

@click.command()
@click.option('--model_name', type=str, help='SSL model name')
@click.option('--input_rspecifier', type=str, help='Input rspecifier')
@click.option('--output_wspecifier', type=str, help='Output wspecifier')
@click.option('--layer', type=int, help='Output layer')
@click.option('--job', type=int, help='Job ID')
def main(model_name, input_rspecifier, output_wspecifier, layer, job):
    device = f'cuda:{job-1}'
    print(f'Using {device} with input {input_rspecifier} output {output_wspecifier}', file=sys.stderr)
    set_dir("/disk/data2/s1569734/globalphone_ssl/s3prl_download")
    model = S3PRLUpstream(model_name, "ckpts").to(device)
    model.eval()
    
    with SequentialWaveReader(input_rspecifier) as reader:
        q = JoinableQueue()
        p = Process(target=writer_process, args=(q, output_wspecifier))
        p.start()

        for name, wav in reader:
            wav = wav.data.numpy()[0]

            chunks = []
            chunk_length = 40
            for start in range(0, wav.shape[0], chunk_length * 16000):
                with torch.no_grad():
                    chunk = torch.FloatTensor(wav[start:start + chunk_length * 16000]).view(1, -1).to(device)
                    chunk_len = torch.LongTensor([chunk.shape[-1]]).view(1, -1).to(device)
                    chunks.append(model(chunk, chunk_len)[0][layer].cpu().numpy()[0])

            features = np.concatenate(chunks)
            q.put((name, features))
            print(f'Extracted feats for {name} with shape {features.shape}', file=sys.stderr)
        
        q.put(None)
        q.join()
        p.join()

if __name__ == '__main__':
    main()
