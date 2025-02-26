# Deciphering Speech: a Zero-Resource Approach to Cross-Lingual Transfer in ASR

This repository contains code for our Interspeech 2022 paper titled ["Deciphering Speech: a Zero-Resource Approach to Cross-Lingual Transfer in ASR"](https://www.isca-archive.org/interspeech_2022/klejch22_interspeech.html).

## Installation
First, we need to compile [Kaldi](https://github.com/kaldi-asr/kaldi)
```
git clone https://github.com/kaldi-asr/kaldi.git
cd kaldi/tools
make -j 8 OPENFST_VERSION=1.7.2 && ./extras/install_srilm.sh && ./extras/install_tcmalloc.sh
cd ../src
./configure --use-cuda=yes --enable-tcmalloc && make depend -j 8 && make -j 8
cd ../..
```

Second, we need to compile our binaries
```
git clone https://github.com/unmute-tech/deciphering_speech.git
cd deciphering_speech
KALDI_ROOT=$PWD/../kaldi bash install.sh
```

Finally, we need to install PyKaldi and S3PRL for extracting features with XLS-R.
```
conda create -y --name deciphering-speech python=3.9
conda activate deciphering-speech
conda install -y -c pytorch -c nvidia pytorch torchvision torchaudio pytorch-cuda=11.8
conda install -y -c kaldi_native_io kaldi_native_io
pip install s3prl click nltk soundfile
```

## Deciphering new language
Given a folder `wavs` with audio files and language iso code and script, we can decipher new language with the following command:
```
./run.sh <lang-iso> <script> <wavs> <work-dir>
```
By default this script uses 40 CPUs and around 64-128GB RAM depending on the language. It also uses 4 GPUs for XLS-R feature extraction.

## Citation
For research using this work, please cite:
```
@inproceedings{klejch2022deciphering,
  author={Ondrej Klejch and Electra Wallington and Peter Bell},
  title={{Deciphering Speech: a Zero-Resource Approach to Cross-Lingual Transfer in ASR}},
  year=2022,
  booktitle={Proc. Interspeech 2022},
  pages={2288--2292},
  doi={10.21437/Interspeech.2022-10170},
  issn={2308-457X}
}
```

## Acknowledgements
This work is supported by the UK Engineering and Physical Sciences Research Council (EPSRC) programme grant: [UnMute](https://unmute.tech/) (EP/T024976/1).
