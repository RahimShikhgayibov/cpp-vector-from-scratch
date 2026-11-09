#pragma once

#include <cstddef>  // std::size_t
#include <utility>  // std::swap

namespace mystl {

// v2: yalnız int saxlayan dinamik massiv + düzgün kopyalama (rule of three).
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
//   hər IntVector öz massivinə SAHİBDİR: iki obyekt eyni data_-nı göstərmir
//
// Rule of three: sinif resursu (heap yaddaşı) özü idarə edirsə, bu üçünü
// özü yazmalıdır: destructor, copy constructor, copy assignment.
class IntVector {
public:
    IntVector() = default;

    // (1) Destructor: sahib olduğu massivi qaytarır.
    ~IntVector() {
        delete[] data_;  // nullptr-i silmək təhlükəsizdir, heç nə etmir
    }

    // (2) Copy constructor: YENİ obyekti başqasının kopyası kimi qurur.
    //     `IntVector b = a;` və ya `IntVector b(a);` yazanda çağırılır.
    //     Deep copy: pointer-i yox, massivin ÖZÜNÜ kopyalayır.
    IntVector(const IntVector& other)
        : data_(other.size_ == 0 ? nullptr : new int[other.size_]),
          size_(other.size_),
          capacity_(other.size_) {  // kopya artıq yer saxlamır: capacity == size
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    // (3) Copy assignment: ARTIQ MÖVCUD obyektə başqasının dəyərini verir.
    //     `a = b;` yazanda çağırılır. Copy-and-swap üsulu:
    //       1. other-in kopyasını müvəqqəti obyektdə qur (copy constructor ilə)
    //       2. öz məzmununu müvəqqəti obyektlə dəyiş
    //       3. funksiyadan çıxanda müvəqqəti obyekt KÖHNƏ məzmunu özü ilə silir
    //     Self-assignment (a = a) da təhlükəsizdir: əvvəl kopya alınır.
    IntVector& operator=(const IntVector& other) {
        IntVector copy(other);  // new burada exception atsa, *this toxunulmaz qalır
        swap(copy);
        return *this;           // a = b = c zəncirini mümkün edir
    }

    // İki vektorun məzmununu dəyişir. Yalnız 3 üzv dəyişir, element kopyalanmır:
    // O(1) və heç vaxt exception atmır (noexcept).
    void swap(IntVector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
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
