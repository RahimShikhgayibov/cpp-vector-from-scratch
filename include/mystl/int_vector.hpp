#pragma once

#include <cstddef>  // std::size_t

namespace mystl {

// v1: yalnız int saxlayan dinamik massiv.
//
// Yaddaş şəkli (size = 3, capacity = 4):
//
//   data_ ──► [ 10 | 20 | 30 | ?? ]
//               0    1    2    3
//              └── size_ ──┘
//              └──── capacity_ ────┘
//
// İnvariant (həmişə doğru olmalıdır):
//   size_ <= capacity_
//   capacity_ == 0  =>  data_ == nullptr
//   data_[0 .. size_-1] istifadəçinin əlavə etdiyi elementlərdir
//
// DİQQƏT: bu versiyada copy constructor və copy assignment YOXDUR.
// `IntVector b = a;` yazsan, hər iki obyekt eyni yaddaşı göstərəcək və
// ikisinin də destructor-u onu silməyə çalışacaq (double free).
// Bu problem A2-də (rule of three) həll olunur.
class IntVector {
public:
    IntVector() = default;

    ~IntVector() {
        delete[] data_;  // nullptr-i silmək təhlükəsizdir, heç nə etmir
    }

    void push_back(int value) {
        if (size_ == capacity_) {
            grow();
        }
        data_[size_] = value;
        ++size_;
    }

    // İki versiya: biri dəyişdirmək üçün, biri const obyektdən oxumaq üçün.
    // Sərhəd yoxlanmır (std::vector::operator[] kimi). Yoxlayan at() B1-dədir.
    int& operator[](std::size_t index) { return data_[index]; }
    const int& operator[](std::size_t index) const { return data_[index]; }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }

private:
    // Tutumu 2 dəfə artırır: 0 -> 1 -> 2 -> 4 -> 8 -> ...
    void grow() {
        const std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;

        int* new_data = new int[new_capacity];   // 1. yeni, böyük yer ayır
        for (std::size_t i = 0; i < size_; ++i) {
            new_data[i] = data_[i];               // 2. köhnə elementləri köçür
        }
        delete[] data_;                           // 3. köhnə yeri azad et
        data_ = new_data;                         // 4. yeni yerə keç
        capacity_ = new_capacity;
    }

    int* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

}  // namespace mystl
