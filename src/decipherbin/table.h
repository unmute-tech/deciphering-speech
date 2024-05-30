#ifndef DECIPHERMENT_TABLE_H_
#define DECIPHERMENT_TABLE_H_

#include <vector>
#include <functional>

template <class T>
class Table {

  public:

    Table(size_t d1, const T &val): d1_(d1), data_(d1, val) {};
    Table(size_t d1, size_t d2, const T &val): d1_(d1), d2_(d2), data_(d1 * d2, val) {};
    Table(size_t d1, size_t d2, size_t d3, const T &val): d1_(d1), d2_(d2), d3_(d3), data_(d1 * d2 * d3, val) {};

    T & operator()(size_t i) {return data_[i];}
    T & operator()(size_t i, size_t j) {return data_[i*d2_ + j];}
    T & operator()(size_t i, size_t j, size_t k) {return data_[i*d2_*d3_ + j*d3_ + k];}

    T const & operator()(size_t i) const {return data_[i];}
    T const & operator()(size_t i, size_t j) const {return data_[i*d2_ + j];}
    T const & operator()(size_t i, size_t j, size_t k) const {return data_[i*d2_*d3_ + j*d3_ + k];}

    void Add(const Table<T> &other, const std::function<T(T, T)> &lambda) {
      for (size_t i = 0; i < data_.size(); i++) {
        data_[i] = lambda(data_[i], other.data_[i]);
      }
    }

    void SetToConstant(const T &val) {
      std::fill(data_.begin(), data_.end(), val);
    }

  private:

    size_t d1_, d2_, d3_;
    std::vector<T> data_;

};

#endif  // DECIPHERMENT_TABLE_H_
