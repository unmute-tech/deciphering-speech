#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "fstext/fstext-utils.h"
#include "fstext/kaldi-fst-io.h"


int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    using namespace fst;
    typedef kaldi::int32 int32;
    typedef kaldi::uint64 uint64;

    const char *usage =
        "Reads a table of FSTs; for each element, finds the best path and \n"
        "prints out the output-symbol sequence (if --output-side=true), or \n"
        "input-symbol sequence otherwise.\n"
        "\n"
        "Usage:\n"
        " transcripts-to-fsts [options] <transcriptions-rspecifier>"
        " <fsts-wspecifier>\n"
        "e.g.:\n"
        " transcripts-to-fsts ark,t:train.text ark:train.fsts\n";

    ParseOptions po(usage);
    po.Read(argc, argv);

    if (po.NumArgs() != 2) {
      po.PrintUsage();
      exit(1);
    }

    std::string transcript_rspecifier = po.GetArg(1),
        fsts_wspecifier = po.GetArg(2);

    SequentialInt32VectorReader transcript_reader(transcript_rspecifier);
    TableWriter<VectorFstHolder> fst_writer(fsts_wspecifier);

    int32 n_done = 0, n_err = 0;
    for (; !transcript_reader.Done(); transcript_reader.Next()) {
      std::string key = transcript_reader.Key();
      const std::vector<int32> &transcript = transcript_reader.Value();

      VectorFst<StdArc> fst;
      MakeLinearAcceptor(transcript, &fst);
      fst_writer.Write(key, fst);
      n_done++;
    }

    KALDI_LOG << "Converted " << n_done << " FSTs, " << n_err << " with errors";
    return (n_done != 0 ? 0 : 1);
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
