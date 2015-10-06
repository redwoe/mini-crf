#ifndef __FEATURES_H__
#define __FEATURES_H__

#include<cmath>
#include"crf.hpp"
#include"speech_synthesis.hpp"

using namespace tool;

#define FEATURES_COUNT 2

enum FunctionId {
  pitch = 0,
  mfcc = 1
};

struct FeatureValues : public std::array<float, FEATURES_COUNT> {
  int diff(const FeatureValues& o) const {
    for(unsigned i = 0; i < size(); i++) {
      if((*this)[i] != o[i])
        return (int) i;
    }
    return -1;
  }

  void populate(float pitch, float mfcc) {
    (*this)[FunctionId::pitch] = pitch;
    (*this)[FunctionId::mfcc] = mfcc;
  }
};

struct Features {
  static cost Pitch(const PhonemeInstance& prev,
                    const PhonemeInstance& next,
                    int,
                    const vector<PhonemeInstance>&) {
    return std::abs(prev.pitch_contour[1] - next.pitch_contour[0]);
  }

  static cost LeftContext(const PhonemeInstance& prev,
                          const PhonemeInstance& next,
                          int,
                          const vector<PhonemeInstance>&) {
    return prev.label != next.ctx_left;
  }

  static cost PitchState(const PhonemeInstance& p1,
                         int pos,
                         const vector<PhonemeInstance>& x) {
    const PhonemeInstance& p2 = x[pos];
    return p1.pitch_contour.diff(p2.pitch_contour);
  }

  static cost MFCCDist(const PhonemeInstance& prev,
                       const PhonemeInstance& next,
                       int, const vector<PhonemeInstance>&) {
    const MfccArray& mfcc1 = prev.last().mfcc;
    const MfccArray& mfcc2 = next.first().mfcc;
    cost result = 0;
    std::array<mfcc_t, MFCC_N> tmp;
    for (unsigned i = 0; i < mfcc1.size(); i++) {
      auto diff = mfcc1[i] - mfcc2[i];
      tmp[i] = diff;
    }
    for (unsigned i = 0; i < mfcc1.size(); i++)
      result += tmp[i] * tmp[i];
  
    return std::sqrt(result);
  }

  static cost MFCCDistL1(const PhonemeInstance& prev,
                         const PhonemeInstance& next,
                         int,
                         const vector<PhonemeInstance>&) {
    const MfccArray& mfcc1 = prev.last().mfcc;
    const MfccArray& mfcc2 = next.first().mfcc;
    mfcc_t result = 0;
    for (unsigned i = 0; i < mfcc1.size(); i++) {
      mfcc_t diff = mfcc1[i] - mfcc2[i];
      result += std::abs(diff);
    }
    return result;
  }

  static cost Duration(const PhonemeInstance& prev,
                       int pos,
                       const vector<PhonemeInstance>& x) {
    const PhonemeInstance& next = x[pos];
    stime_t d1 = prev.end - prev.start;
    stime_t d2 = next.end - next.start;
    return std::abs(d1 - d2);
  }

  static cost BaselineFunction(const PhonemeInstance& prev,
                               const PhonemeInstance& next,
                               int,
                               const vector<PhonemeInstance>&) {
    return (prev.id + 1 == next.id) ? 0 : 1;
  }

  template<class Eval>
  cost feature_value(const PhonemeInstance& p1, const PhonemeInstance& p2) {
    Eval eval;
    vector<PhonemeInstance> empty;
    return eval(p1, p2, 0, empty);
  }

  FeatureValues get_feature_values(const PhonemeInstance& p1, const PhonemeInstance& p2) {
    FeatureValues result;
    vector<PhonemeInstance> dummy;
    result.populate(
                    Pitch(p1, p2, 0, dummy),
                    MFCCDist(p1, p2, 0, dummy)
                    );
    return result;
  }
};

struct PhoneticFeatures;
typedef CRandomField<PhonemeAlphabet, PhonemeInstance, PhoneticFeatures> CRF;

struct PhoneticFeatures {
  typedef cost (*_EdgeFeature)(const PhonemeInstance&, const PhonemeInstance&, int, const vector<PhonemeInstance>&);
  typedef cost (*_VertexFeature)(const PhonemeInstance&, int, const vector<PhonemeInstance>&);

  const std::string enames[3] = {
    "trans-pitch",
    "trans-mfcc",
    "tranx-ctx"
  };
  const _EdgeFeature f[3] = {
    Features::Pitch,
    Features::MFCCDist,
    Features::LeftContext
  };

  const std::string vnames[2] = {
    "state-duration",
    "state-pitch"
  };
  const _VertexFeature g[2] = {
    Features::Duration,
    Features::PitchState
  };
};

struct BaselineFeatures;
typedef CRandomField<PhonemeAlphabet, PhonemeInstance, BaselineFeatures> BaselineCRF;

struct BaselineFeatures {
  typedef cost (*_EdgeFeature)(const PhonemeInstance&, const PhonemeInstance&, int, const vector<PhonemeInstance>&);
  typedef cost (*_VertexFeature)(const PhonemeInstance&, int, const vector<PhonemeInstance>&);

  const std::string enames[1] = {
    "trans-baseline"
  };
  const _EdgeFeature f[1] = { Features::BaselineFunction };

  const std::string vnames[0] = {};
  const _VertexFeature g[0] = { };
};

#endif
