#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include<iostream>
#include<cmath>
#include<cstring>
#include<limits>
#include<vector>

#define DEBUG(x)
#define MY_E 2.71828182845904523536028747135266250 // e

template<class Key, class Value>
struct Pair {
  Pair(const Key& key, const Value& value): key(key), value(value) { }
  const Key key;
  const Value value;
};

namespace util {
  float mult_exp(float x, float y);
  float mult(float x, float y);
  float log_mult(float logX, float logY);
  float log_sum(float logX, float logY);
  float log_sum2(float logX, float logY);
  float sum_log(float x, float y);
  float sum(float x, float y);

  int parse_int(const std::string& str);
};

bool compare(std::string &, std::string&);
bool compare(int& i1, int& i2);

template<class T>
bool compare(std::vector<T> &v1, std::vector<T> &v2) {
  bool same = v1.size() == v2.size();
  for(unsigned i = 0; i < v1.size(); i++)
    same = same && v1[i] == v2[i];
  return same;
}

struct BinaryWriter {
  BinaryWriter(std::ostream* str): s(str), bytes(0) { }
  std::ostream* const s;
  unsigned bytes;

  template<class T>
  BinaryWriter& w(const T& val) {
    unsigned size = sizeof(val);
    s->write((char*) &val, size);
    bytes += size;
    return *this;
  }

  template<class T>
  BinaryWriter& operator<<(const T& val) {
    return w(val);
  }
};

struct BinaryReader {
  BinaryReader(std::istream* str): s(str), bytes(0) { }
  std::istream* const s;
  unsigned bytes;

  template<class T>
  BinaryReader& r(T& val) {
    unsigned size = sizeof(val);
    s->read((char*) &val, size);
    bytes += size;
    return *this;
  }

  template<class T>
  BinaryReader& operator>>(T& val) {
    return r(val);
  }
};

struct Progress {
  Progress(unsigned total): total(total), progress(0) { }
  unsigned total, progress;

  void update() {
    //std::cerr << '\r' << "Progress: " << progress++ << '/' << total;
    //std::cerr.flush();
  }
  
  void finish() {
    //std::cerr << " Done" << std::endl;
  }
};

#endif
