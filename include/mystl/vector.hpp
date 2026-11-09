#pragma once

#include <cstddef>  // std::size_t
#include <utility>  // std::swap

namespace mystl {

// v3: istənilən T tipini saxlayan dinamik massiv (class template).
//
// Yaddaş şəkli (size = 3, capacity = 4):
//
//   data_ ──► [ e0 | e1 | e2 | ?? ]
//               0    1    2    3
//              └── size_ ──┘
//              └──── capacity_ ────┘
//
// İnvariant (həmişə doğru olmalıdır):
//   size_ <= capacity_
//   capacity_ == 0  =>  data_ == nullptr
//   data_[0 .. size_-1] istifadəçinin əlavə etdiyi elementlərdir
//   hər vector öz massivinə SAHİBDİR: iki obyekt eyni data_-nı göstərmir
//
// Bu versiyanın bilinən məhdudiyyəti (A4-də həll olunur):
//   new T[n] bütün n yer üçün T-nin default constructor-unu çağırır.
//   Ona görə T default constructor-a malik olmalıdır və capacity-dəki
//   "boş" yerlər də əslində qurulmuş obyektlərdir.
template <typename T>
class vector {
public:
    // Tip adları (std::vector-da olduğu kimi). Ümumi kod bunlarla yazılır:
    // məsələn, `typename V::value_type` istənilən konteynerin element tipini verir.
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    vector() = default;

    // (1) Destructor
    ~vector() {
        delete[] data_;  // hər elementin destructor-unu çağırır, sonra yaddaşı qaytarır
    }

    // (2) Copy constructor: deep copy
    vector(const vector& other)
        : data_(other.size_ == 0 ? nullptr : new T[other.size_]),
          size_(other.size_),
          capacity_(other.size_) {
        for (size_type i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];  // T-nin copy assignment-i
        }
    }

    // (3) Copy assignment: copy-and-swap
    vector& operator=(const vector& other) {
        vector copy(other);
        swap(copy);
        return *this;
    }

    void swap(vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    // const T& ilə qəbul edir: T böyük ola bilər (məs. std::string),
    // dəyərlə ötürmək lazımsız bir kopya da yaradardı.
    void push_back(const T& value) {
        if (size_ == capacity_) {
            grow();
        }
        data_[size_] = value;
        ++size_;
    }

    reference operator[](size_type index) { return data_[index]; }
    const_reference operator[](size_type index) const { return data_[index]; }

    size_type size() const { return size_; }
    size_type capacity() const { return capacity_; }

private:
    void grow() {
        const size_type new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;

        T* new_data = new T[new_capacity];          // new_capacity ədəd default T qurur
        for (size_type i = 0; i < size_; ++i) {
            new_data[i] = data_[i];                 // KOPYALAYIR (A5-də move olacaq)
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }

    T* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
};

}  // namespace mystl
