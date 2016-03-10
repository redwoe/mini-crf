#include<chrono>
#include<thread>
#include<valarray>

#include"gridsearch.hpp"
#include"threadpool.h"
#include"crf.hpp"
#include"tool.hpp"
#include"parser.hpp"

//#define DEBUG_TRAINING

extern double hann(double i, int size);

static bool APPLY_WINDOW_CMP = false;
static float WINDOW_OVERLAP = 0.1;

static constexpr auto FC = PhoneticFeatures::size;
struct Params : std::valarray<coefficient> {
  Params(): std::valarray<coefficient>(FC) { }
};

struct Range {
  Range(): Range("", 0, 0, 1) { }
  Range(std::string feature, double from, double to, double step)
    :from(from), to(to), step(step), current(from), feature(feature) {
    assert(step > 0 && from <= to);
  }

  bool has_next() const { return (current + step) <= to; }
  void next() { current += step; }

  void reset() { current = from; }

  double from, to, step, current;
  std::string feature;

  std::string to_string() const {
    std::stringstream str;
    str << "[" << feature << ", " << from << ", " << to << ", " << step << "]";
    return str.str();
  }
};
typedef std::array<Range, FC> Ranges;

template<int FeatureCount>
struct _ValueCache {
  _ValueCache(std::string path): path(path) {
    init();
  }

  std::string path;
  std::vector<gridsearch::Comparisons> values;
  std::vector<std::array<double, FeatureCount> > args;

  bool load(std::array<Range, FeatureCount>& ranges, gridsearch::Comparisons& result) const {
    for(unsigned i = 0; i < args.size(); i++) {
      bool found = true;
      for(unsigned j = 0; j < args[i].size(); j++)
        found = found && (args[i][j] == ranges[j].current);
      if(found) {
        result = values[i];
        return true;
      }
    }
    return false;
  }

  void save(std::array<Range, FeatureCount>& ranges, const gridsearch::Comparisons& result) {
    values.push_back(result);
    std::array<double, FeatureCount> new_args;
    for(unsigned i = 0; i < ranges.size(); i++)
      new_args[i] = ranges[i].current;
    args.push_back(new_args);
  }

  void persist() {
    std::ofstream str(path);
    BinaryWriter w(&str);
    unsigned size = values.size();
    w << size;
    for(unsigned i = 0; i < size; i++) {
      for(unsigned j = 0; j < FeatureCount; j++)
        w << args[i][j];
      w << values[i].ItakuraSaito;
      w << values[i].LogSpectrum;
    }
    INFO("Persisted " << size << " values in " << path);
  }

private:
  void init() {
    std::ifstream str(path);
    BinaryReader reader(&str);
    unsigned size; reader >> size;
    if(!reader.ok()) {
      INFO("No existing value cache");
      return;
    }
    INFO("Values in cache " << path << " " << size);
    for(unsigned i = 0; i < size; i++) {
      std::array<double, FeatureCount> args;
      gridsearch::Comparisons values;
      double arg;
      for(unsigned j = 0; j < FeatureCount; j++) {
        reader >> arg;
        args[j] = arg;
      }
      reader >> values.ItakuraSaito;
      reader >> values.LogSpectrum;
      this->args.push_back(args);
      this->values.push_back(values);
    }
  }
};
typedef _ValueCache<FC> ValueCache;

struct TrainingOutput {
  TrainingOutput(Ranges ranges, gridsearch::Comparisons result)
    :ranges(ranges), result(result)
  { }

  Ranges ranges;
  gridsearch::Comparisons result;
};

namespace gridsearch {

  std::vector<FrameFrequencies> toFFTdFrames(Wave& wave) {
    auto frameOffset = 0u;
    std::vector<FrameFrequencies> result;

    std::valarray<cdouble> buffer(gridsearch::FFT_SIZE);
    FrameFrequencies values;

    auto overlap = 0u;
    while((frameOffset + buffer.size()
           - ((frameOffset != 0) * overlap) < wave.length())) {
      // i.e. after the first frame...
      overlap = buffer.size() * WINDOW_OVERLAP;
      if(frameOffset > 0)
        frameOffset -= overlap;

      WaveData frame = wave.extractBySample(frameOffset, frameOffset + buffer.size());
      frameOffset += frame.size();
      auto i = 0u;
      for(; i < frame.size(); i++) buffer[i] = frame[i];
      for(; i < buffer.size(); i++) buffer[i] = 0;

      if(APPLY_WINDOW_CMP) {
        for(auto j = 0u; j < frame.size(); j++)
          buffer[j] *= hann(j, frame.size());
      }

      ft::fft(buffer);
      for(auto j = 0u; j < buffer.size(); j++)
        values[j] = buffer[j];

      result.push_back(values);
    }
    assert(result.size() > 0);
    return result;
  }

  // TODO
  double compare_IS(Wave& result, Wave& original) {
    return 0;
    double value = 0;
    each_frame(result, original, 0.05, [&](WaveData, WaveData) {
        value++;
      });
    assert(false); // ItakuraSaito not yet implemented
    return value;
  }

  //__attribute__ ((optnone))
  double compare_LogSpectrum(Wave& result, std::vector<FrameFrequencies>& frames2) {
    auto frames1 = toFFTdFrames(result);
    assert(std::abs((int) frames1.size() - (int) frames2.size()) <= 2);

    int minSize = std::min(frames1.size(), frames2.size());
    double value = 0;
    for(auto j = 0; j < minSize; j++) {
      double diff = 0;
      auto& freqs1 = frames1[j];
      auto& freqs2 = frames2[j];
      for(auto i = 0; i < minSize; i++) {
        auto m1 = std::norm(freqs1[i]);
        m1 = m1 ? m1 : 1;
        auto m2 = std::norm(freqs2[i]);
        m2 = m2 ? m2 : 1;
        diff += std::pow( std::log10( m2 / m1), 2 );
      }
      value += diff;
    }
    return value / minSize;
  }

  double compare_LogSpectrum(Wave& result, Wave& original) {
    auto frames2 = toFFTdFrames(original);
    return compare_LogSpectrum(result, frames2);
  }

  std::string to_text_string(const std::vector<PhonemeInstance>& vec) {
    std::string result(1, ' ');
    for(auto it = vec.begin(); it != vec.end(); it++) {
      result.push_back('|');
      result.append(labels_all.convert((*it).label) );
      result.push_back('|');
    }
    return result;
  }

  void do_resynth_index(ResynthParams* params) {
    int index = params->index;
    const std::vector<PhonemeInstance>& input = corpus_test.input(index);
    std::vector<int> path;

    traverse_automaton<MinPathFindFunctions>(input, crf, crf.lambda, &path);
    std::vector<PhonemeInstance> output = crf.alphabet().to_phonemes(path);

    Wave resultSignal = SpeechWaveSynthesis(output, input, crf.alphabet())
      .get_resynthesis(Options{});

    const FileData fileData = alphabet_test.file_data_of(input[0]);
    Wave sourceSignal;
    sourceSignal.read(fileData.file);

    auto& frames = *(params->precompFrames);

    params->result.ItakuraSaito = compare_IS(resultSignal, sourceSignal);
    params->result.LogSpectrum = compare_LogSpectrum(resultSignal, frames[index]);
  }

  void resynth_index(ResynthParams* params) {
    do_resynth_index(params);
    *(params->flag) = 1;
  }

  void wait_done(bool* flags, unsigned count) {
    auto done = false;
    while(!done) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      done = true;
      for(unsigned i = 0; i < count; i++)
        done &= flags[i];
      DEBUG(
            cerr << "Unfinished: ";
            for(unsigned i = 0; i < count; i++)
              if(!flags[i])
                cerr << i << " ";
            cerr << std::endl;
            );
    }
  }

  void aggregate(std::vector<Comparisons> params,
                 Comparisons* sum=0,
                 Comparisons* max=0,
                 Comparisons* avg=0) {
    Comparisons sumTemp;
    Comparisons avgTemp;
    int maxIndex = -1;
    for(unsigned i = 0; i < params.size(); i++) {
      if(maxIndex == -1 || params[i] < params[maxIndex])
        maxIndex = i;
      sumTemp = sumTemp + params[i];
    }
    avgTemp.LogSpectrum = sumTemp.LogSpectrum / params.size();
    avgTemp.ItakuraSaito = sumTemp.ItakuraSaito / params.size();
    if(sum)
      *sum = sumTemp;
    if(max)
      *max = params[maxIndex];
    if(avg)
      *avg = avgTemp;
  }

  double randDouble() {
    static int c = 0;
    c++;
    return - (c * c) % 357;
  }

  struct FFTPrecomputeParams {
    void init(int index, bool* flag, std::string file, std::vector< std::vector<FrameFrequencies> >* precompFrames) {
      this->index = index;
      this->flag = flag;
      this->file = file;
      this->precompFrames = precompFrames;
    }

    std::string file;
    int index;
    bool* flag;

    std::vector< std::vector<FrameFrequencies> >* precompFrames;
  };

  void precomputeSingleFrames(FFTPrecomputeParams* params) {
    Wave sourceSignal;
    sourceSignal.read(params->file);
    auto frames = toFFTdFrames(sourceSignal);
    (*params->precompFrames)[params->index] = frames;
    *(params->flag) = 1;
  }

  void precomputeFrames(std::vector< std::vector<FrameFrequencies> >& precompFrames,
                        ThreadPool& tp) {
    unsigned count = corpus_test.size();
    bool flags[count];

    FFTPrecomputeParams *params = new FFTPrecomputeParams[count];
    for(unsigned i = 0; i < count; i++) {
      flags[i] = 0;
      FileData fileData = alphabet_test.file_data_of(corpus_test.input(i)[0]);

      params[i].init(i, &flags[i], fileData.file, &precompFrames);

      Task* t = new ParamTask<FFTPrecomputeParams>(&precomputeSingleFrames, &params[i]);
      tp.add_task(t);
    }
    wait_done(flags, count);

    delete[] params;
  }

  Comparisons do_train(ThreadPool& tp,
                       std::vector< std::vector<FrameFrequencies> > *precompFrames) {
#ifdef DEBUG_TRAINING
    return Comparisons().dummy(randDouble());
#endif
    unsigned count = corpus_test.size();
    bool flags[count];
    for(unsigned i = 0; i < count; i++)
      flags[i] = 0;

    auto params = new ResynthParams[count];
    for(unsigned i = 0; i < count; i++) {
      params[i].init(i, &flags[i]);

      params[i].precompFrames = precompFrames;

      Task* t = new ParamTask<ResynthParams>(&resynth_index, &params[i]);
      tp.add_task(t);
    }
    wait_done(flags, count);

    std::vector<Comparisons> comps;
    for(unsigned i = 0; i < count; i++) comps.push_back(params[i].result);

    Comparisons result;
    aggregate(comps, 0, 0, &result);
    delete[] params;
    return result;
  }

  void csvPrintHeaders(std::ofstream& csvOutput, std::string& csvFile,
                       Ranges& ranges) {
    if(csvFile != std::string("")) {
      util::join_output(csvOutput, ranges, [](const Range& r) { return r.feature; }, " ")
        << " value" << std::endl;
    }
  }

  void csvPrint(std::ofstream& csvOutput, std::string& csvFile,
                Ranges& ranges, Comparisons& result) {
    if(csvFile != std::string("")) {
      util::join_output(csvOutput, ranges, [](const Range& r) { return r.current; }, " ")
        << " " << result.value() << std::endl;
    }
  }

  struct BruteSearch {
    BruteSearch(int maxPasses): maxPasses(maxPasses) {}
    int maxPasses, pass = 0;
    void bootstrap(Ranges& ranges, Params& current,
                   Params& delta, Params& p_delta) {
      for(auto i = 0u; i < ranges.size(); i++) {
        current[i] = ranges[i].from;
        delta[i] = p_delta[i] = 0;
      }
      current[1] = 1;
      delta[1] = 1;
    }

    bool nextStep(Ranges& ranges, Params& current,
                  Params& delta, Params& p_delta) {
      // last non-0 index
      auto i = 0u;
      while(i < delta.size() && delta[i] == 0)
        i++;

      if(current[i] < ranges[i].to) {
        // Do nothing, step and axis don't change
        INFO(ranges[i].feature << ": " << current[i] << " -> " << (current[i] + delta[i]));
      } else {
        // Stop moving along this axis
        delta[i] = 0;
        // Pick next axis
        auto nextIndex = (i + 1) % delta.size();
        // Pick density of new axis
        delta[nextIndex] = ranges[nextIndex].step;
        // Reset value along new axis
        current[nextIndex] = ranges[nextIndex].from;

        INFO(ranges[i].feature << " -> " << ranges[nextIndex].feature);

        if(nextIndex == 0)
          pass++;
      }

      current += delta;
      return pass == maxPasses && i == delta.size();
    }
  };

  template<class State>
  void descentSearch(State state,
                     Ranges& ranges,
                     int maxIterations,
                     std::vector< std::vector<FrameFrequencies> > &precompFrames,
                     ThreadPool& tp,
                     ValueCache&,
                     std::string csvFile) {
    std::ofstream csvOutput(csvFile);
    csvPrintHeaders(csvOutput, csvFile, ranges);

    Params current, bestParams,
      delta, p_delta;

    state.bootstrap(ranges, current, delta, p_delta);
    for(auto i = 0u; i < ranges.size(); i++)
      crf.set(ranges[i].feature, current[i]);

    delta[0] = 1;
    auto iteration = 1;
    INFO("Iteration " << iteration++);
    auto result = do_train(tp, &precompFrames),
      bestResult = result;
    INFO("Value: " << result.value());

    bool stop = state.nextStep(ranges, current, delta, p_delta);
    while(iteration <= maxIterations && !stop) {
      INFO("Iteration " << iteration++);
      csvPrint(csvOutput, csvFile, ranges, result);

      for(auto i = 0u; i < ranges.size(); i++)
        crf.set(ranges[i].feature, current[i]);

      result = do_train(tp, &precompFrames);
      INFO("Value: " << result.value());

      if(result.LogSpectrum < bestResult.LogSpectrum) {
        bestResult = result;
        bestParams = current;
      }

      stop = state.nextStep(ranges, current, delta, p_delta);
    }
  }

  int train(const Options& opts) {
    Progress::enabled = false;
    Comparisons::metric = opts.get_opt<std::string>("metric", "");
    APPLY_WINDOW_CMP = opts.get_opt("apply-window-cmp", false);
    WINDOW_OVERLAP = opts.get_opt("window-overlap", 0.1);

    //#pragma omp parallel for
    int threads = opts.get_opt<int>("thread-count", 8);
    ThreadPool tp(threads);
    int ret = tp.initialize_threadpool();
    if (ret == -1) {
      ERROR("Failed to initialize thread pool");
      return ret;
    }

    Ranges ranges = {{
        Range("trans-ctx", 0, 300, 1),
        Range("trans-pitch", 0, 300, 1),
        Range("state-pitch", 0, 300, 1),
        Range("trans-mfcc", 0, 2, 0.01),
        Range("state-duration", 0, 300, 1),
        Range("state-energy", 0, 300, 1)
      }};
    for(auto& it : ranges)
      INFO("Range " << it.to_string());

    std::string valueCachePath = opts.get_opt<std::string>("value-cache", "value-cache.bin");
    ValueCache vc(valueCachePath);

    // Pre-compute FFTd frames of source signals
    INFO("Precomputing FFTs");
    std::vector< std::vector<FrameFrequencies> > precompFrames;
    precompFrames.resize(corpus_test.size());

#ifndef DEBUG_TRAINING
    precomputeFrames(precompFrames, tp);
#endif
    INFO("Done");

    std::vector<TrainingOutput> outputs;
    unsigned passes = opts.get_opt<int>("training-passes", 3);
    int maxIterations = opts.get_opt<int>("max-iterations", 9999999);
    std::string csvFile = opts.get_string("csv-file");

    descentSearch(BruteSearch(passes), ranges, maxIterations, precompFrames, tp, vc, csvFile);

    INFO("Best at: ");
    for(unsigned k = 0; k < ranges.size(); k++)
      LOG(ranges[k].feature << "=" << ranges[k].current);

    return 0;
  }
}
