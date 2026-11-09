#pragma once

#include <cstddef>  // std::size_t
#include <limits>   // std::numeric_limits
#include <memory>   // std::construct_at, std::destroy_at, std::destroy_n, std::uninitialized_copy_n
#include <new>      // ::operator new, std::align_val_t, std::bad_array_new_length
#include <utility>  // std::swap

namespace mystl {

// v4: istənilən T üçün dinamik massiv, yaddaş və obyektlər AYRI idarə olunur.
//
// Yaddaş modeli (size = 3, capacity = 5):
//
//   data_ ──► [ e0 | e1 | e2 | ░░ | ░░ ]
//              └ qurulmuş ┘   └ xam ─┘
//
//   qurulmuş: canlı T obyektləri, [0, size_)
//   xam:      T üçün ayrılmış, amma içində obyekt OLMAYAN yaddaş, [size_, capacity_)
//
// İnvariant (həmişə doğru olmalıdır):
//   size_ <= capacity_
//   capacity_ == 0  =>  data_ == nullptr
//   [0, size_) yerlərində tam olaraq size_ canlı obyekt var, [size_, capacity_) xamdır
//   hər vector öz buffer-inə SAHİBDİR: iki obyekt eyni data_-nı göstərmir
//
// Əsas qayda: obyekt yalnız istifadəçi element əlavə edəndə qurulur
// (std::construct_at) və element silinəndə dərhal dağıdılır (std::destroy_at).
// Yaddaş isə ayrıca alınır (::operator new) və qaytarılır (::operator delete).
//
// T-dən tələb olunan: copy constructor və destructor. Default constructor
// artıq LAZIM DEYİL.
template <typename T>
class vector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    vector() = default;

    // (1) Destructor: əvvəl canlı obyektləri dağıt, sonra yaddaşı qaytar.
    ~vector() {
        std::destroy_n(data_, size_);
        deallocate(data_);
    }

    // (2) Copy constructor: yalnız size_ qədər yer alır və orada kopyaları qurur.
    //     Exception olsa, copy_to_new_buffer heç nə sızdırmır və bu obyekt
    //     heç vaxt yaranmamış sayılır.
    vector(const vector& other) {
        if (other.size_ == 0) {
            return;
        }
        data_ = copy_to_new_buffer(other.data_, other.size_, other.size_);
        size_ = other.size_;
        capacity_ = other.size_;
    }

    // (3) Copy assignment: copy-and-swap (A2-dəki kimi)
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

    void push_back(const T& value) {
        if (size_ < capacity_) {
            // Yer var: ilk xam yerdə yeni obyekt qur.
            std::construct_at(data_ + size_, value);
            ++size_;
            return;
        }

        // Yer yoxdur: daha böyük buffer lazımdır.
        //
        // DİQQƏT: value köhnə buffer-in öz elementi ola bilər: v.push_back(v[0]).
        // Ona görə yeni elementi köhnə buffer dağıdılmamışdan ƏVVƏL qururuq.
        // (v3-də bu bug idi: köhnə massiv silinəndən sonra value oxunurdu.)
        const size_type new_capacity = next_capacity();
        T* new_data = allocate(new_capacity);
        try {
            std::construct_at(new_data + size_, value);
        } catch (...) {
            deallocate(new_data);
            throw;
        }
        try {
            std::uninitialized_copy_n(data_, size_, new_data);
        } catch (...) {
            std::destroy_at(new_data + size_);
            deallocate(new_data);
            throw;
        }
        replace_buffer(new_data, new_capacity);
        ++size_;
    }

    // Son elementi dağıdır. Yaddaş qalır (capacity dəyişmir).
    // Boş vektorda çağırmaq undefined behavior-dur (std::vector-da da belədir).
    void pop_back() {
        --size_;
        std::destroy_at(data_ + size_);
    }

    // Ən azı new_capacity elementlik yer ayırır. Heç bir obyekt QURMUR.
    // new_capacity hazırkı capacity-dən kiçik və ya bərabərdirsə, heç nə etmir.
    void reserve(size_type new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }
        T* new_data = copy_to_new_buffer(data_, size_, new_capacity);
        replace_buffer(new_data, new_capacity);
    }

    reference operator[](size_type index) { return data_[index]; }
    const_reference operator[](size_type index) const { return data_[index]; }

    size_type size() const { return size_; }
    size_type capacity() const { return capacity_; }

private:
    // --- Xam yaddaş ---------------------------------------------------------

    // Adi ::operator new __STDCPP_DEFAULT_NEW_ALIGNMENT__ (x86-64-də 16 bayt)
    // qədər hizalama zəmanət edir. alignas(64) kimi tiplər üçün xüsusi versiya lazımdır.
    static constexpr bool over_aligned = alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    // n ədəd T üçün XAM yaddaş. Heç bir constructor çağırılmır.
    static T* allocate(size_type n) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_array_new_length();  // n * sizeof(T) daşardı (overflow)
        }
        void* raw = nullptr;
        if constexpr (over_aligned) {
            raw = ::operator new(n * sizeof(T), std::align_val_t{alignof(T)});
        } else {
            raw = ::operator new(n * sizeof(T));
        }
        return static_cast<T*>(raw);
    }

    // allocate-in qaytardığı yaddaşı geri verir. Destructor ÇAĞIRMIR.
    // nullptr ötürmək təhlükəsizdir.
    static void deallocate(T* p) noexcept {
        if constexpr (over_aligned) {
            ::operator delete(p, std::align_val_t{alignof(T)});
        } else {
            ::operator delete(p);
        }
    }

    // --- Köməkçilər ---------------------------------------------------------

    // capacity-lik yeni buffer alır və [first, first + count) elementlərinin
    // kopyalarını onun əvvəlində qurur. Exception olsa, heç nə sızmır:
    // uninitialized_copy_n artıq qurduqlarını özü dağıdır, biz də yaddaşı qaytarırıq.
    static T* copy_to_new_buffer(const T* first, size_type count, size_type capacity) {
        T* new_data = allocate(capacity);
        try {
            std::uninitialized_copy_n(first, count, new_data);
        } catch (...) {
            deallocate(new_data);
            throw;
        }
        return new_data;
    }

    // Köhnə buffer-i (obyektləri və yaddaşı) atıb yenisinə keçir.
    // Yeni buffer artıq tam hazır olmalıdır. Exception atmır.
    void replace_buffer(T* new_data, size_type new_capacity) noexcept {
        std::destroy_n(data_, size_);
        deallocate(data_);
        data_ = new_data;
        capacity_ = new_capacity;
    }

    size_type next_capacity() const {
        return (capacity_ == 0) ? 1 : capacity_ * 2;
    }

    T* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
};

}  // namespace mystl
