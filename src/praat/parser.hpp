#ifndef __PRAAT_PARSER_H__
#define __PRAAT_PARSER_H__

#include<iostream>
#include<vector>

#include"../praat/matrix.hpp"
#include"../crf/util.hpp"

static const int MFCC_N = 12;

typedef FixedArray<double, MFCC_N> MfccArray;

struct Frame {
  Frame():pitch(0) { init(); }
  Frame(double pitch): pitch(pitch) { init(); }

  MfccArray mfcc;
  double pitch;

private:
  void init() { for(unsigned i = 0; i < mfcc.length(); i++) mfcc[i] = 0; }
};

struct PhonemeInstance {
  PhonemeInstance() {
    start = 0;
    end = 0;
    label = ' ';
    id = 0;
  }
  Array<Frame> frames;
  double start;
  double end;
  char label;
  unsigned id;

  unsigned size() const { return frames.length; }

  double duration() const { return end - start; }
  const Frame& at(unsigned index) const { return frames[index]; }
  const Frame& first() const { return frames[0]; }
  const Frame& last() const { return frames[size() - 1]; }
};

static const unsigned PITCH_CONTOUR_LENGTH = 1;
struct PitchContour {
  unsigned length() { return PITCH_CONTOUR_LENGTH; }
  double values[PITCH_CONTOUR_LENGTH];

  double diff(const PitchContour& other) {
    double result = 0;
    for(unsigned i = 0; i < length(); i++) {
      result += std::abs( std::log( values[i] / other.values[i] ) );
    }
    return result;
  }
};

PitchContour to_pitch_contour(const PhonemeInstance&);

void print_synth_input_csv(std::ostream&, std::vector<PhonemeInstance>&);
std::vector<PhonemeInstance> parse_synth_input_csv(std::istream&);

BinaryWriter& operator<<(BinaryWriter&, const PhonemeInstance&);
BinaryReader& operator>>(BinaryReader&, PhonemeInstance&);

PhonemeInstance* parse_file(std::istream&, int&);

bool compare(Frame&, Frame&);
bool compare(PhonemeInstance&, PhonemeInstance&);

bool operator==(const PhonemeInstance&, const PhonemeInstance&);
#endif
