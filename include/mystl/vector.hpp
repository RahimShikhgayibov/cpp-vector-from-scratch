#pragma once

#include <cstddef>  // std::size_t
#include <limits>   // std::numeric_limits
#include <memory>   // std::construct_at, std::destroy_at, std::destroy_n, std::uninitialized_copy_n
#include <new>      // ::operator new, std::align_val_t, std::bad_array_new_length
#include <utility>  // std::move, std::forward, std::exchange, std::move_if_noexcept, std::swap

namespace mystl {

// v5: istənilən T üçün dinamik massiv. Yaddaş və obyektlər ayrı idarə olunur,
// elementlər mümkün olanda KÖÇÜRÜLÜR (move), kopyalanmır.
//
// Yaddaş modeli (size = 3, capacity = 5):
//
//   data_ ──► [ e0 | e1 | e2 | ░░ | ░░ ]
//              └ qurulmuş ┘   └ xam ─┘
//
// İnvariant (həmişə doğru olmalıdır):
//   size_ <= capacity_
//   capacity_ == 0  =>  data_ == nullptr
//   [0, size_) yerlərində tam olaraq size_ canlı obyekt var, [size_, capacity_) xamdır
//   hər vector öz buffer-inə SAHİBDİR: iki obyekt eyni data_-nı göstərmir
//
// Rule of five: destructor, copy constructor, copy assignment,
//               move constructor, move assignment.
//
// T-dən tələb olunan: destructor və (copy VƏ YA move) constructor.
// Yalnız köçürülə bilən tiplər də işləyir (məs. std::unique_ptr), sadəcə
// vector-un özü onda kopyalana bilməz.
template <typename T>
class vector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    vector() = default;

    // (1) Destructor
    ~vector() {
        std::destroy_n(data_, size_);
        deallocate(data_);
    }

    // (2) Copy constructor: deep copy, yalnız size_ qədər yer.
    vector(const vector& other) {
        if (other.size_ == 0) {
            return;
        }
        data_ = copy_to_new_buffer(other.data_, other.size_, other.size_);
        size_ = other.size_;
        capacity_ = other.size_;
    }

    // (3) Copy assignment: copy-and-swap
    vector& operator=(const vector& other) {
        vector copy(other);
        swap(copy);
        return *this;
    }

    // (4) Move constructor: other-in buffer-ini OĞURLAYIR. Element nə kopyalanır,
    //     nə köçürülür: yalnız 3 üzv dəyişir, yəni O(1). other boş qalır.
    //     noexcept: yaddaş ayrılmır, exception mümkün deyil. Bu, vacibdir:
    //     vector<vector<T>> böyüyəndə daxili vektorlar yalnız noexcept olduqda köçürülür.
    vector(vector&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          size_(std::exchange(other.size_, 0)),
          capacity_(std::exchange(other.capacity_, 0)) {}

    // (5) Move assignment: move-and-swap.
    //     1. other-in məzmunu müvəqqəti obyektə köçür (other boşalır)
    //     2. öz məzmunumuzu müvəqqəti obyektlə dəyiş
    //     3. funksiyadan çıxanda müvəqqəti obyekt KÖHNƏ məzmunumuzu silir
    //     Self-move (v = std::move(v)) da təhlükəsizdir: v dəyişmədən qalır.
    vector& operator=(vector&& other) noexcept {
        vector moved(std::move(other));
        swap(moved);
        return *this;
    }

    void swap(vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    // lvalue: arqument kopyalanır.  v.push_back(s);
    void push_back(const T& value) { append(value); }

    // rvalue: arqument köçürülür.    v.push_back(std::move(s));  v.push_back(T(...));
    void push_back(T&& value) { append(std::move(value)); }

    // Son elementi dağıdır. Yaddaş qalır (capacity dəyişmir).
    // Boş vektorda çağırmaq undefined behavior-dur (std::vector-da da belədir).
    void pop_back() {
        --size_;
        std::destroy_at(data_ + size_);
    }

    // Ən azı new_capacity elementlik yer ayırır, mövcud elementləri köçürür.
    // Heç bir yeni obyekt QURMUR. Kiçik və ya bərabər dəyər üçün heç nə etmir.
    void reserve(size_type new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }
        T* new_data = allocate(new_capacity);
        try {
            relocate_into(new_data);
        } catch (...) {
            deallocate(new_data);
            throw;
        }
        replace_buffer(new_data, new_capacity);
    }

    reference operator[](size_type index) { return data_[index]; }
    const_reference operator[](size_type index) const { return data_[index]; }

    size_type size() const { return size_; }
    size_type capacity() const { return capacity_; }

private:
    // --- Element əlavə etmək -------------------------------------------------

    // Hər iki push_back-in ortaq kodu. U&& burada "forwarding reference"-dir:
    // lvalue gələndə U = const T&, rvalue gələndə U = T olur.
    // std::forward<U>(value) dəyərin əvvəlki növünü (lvalue/rvalue) bərpa edir:
    // lvalue kopyalanır, rvalue köçürülür. (Ətraflı: B4, emplace_back.)
    template <typename U>
    void append(U&& value) {
        if (size_ < capacity_) {
            std::construct_at(data_ + size_, std::forward<U>(value));
            ++size_;
            return;
        }

        // Yer yoxdur. value köhnə buffer-in öz elementi ola bilər: v.push_back(v[0]).
        // Ona görə yeni elementi köhnə buffer dağıdılmamışdan ƏVVƏL qururuq.
        const size_type new_capacity = next_capacity();
        T* new_data = allocate(new_capacity);
        try {
            std::construct_at(new_data + size_, std::forward<U>(value));
        } catch (...) {
            deallocate(new_data);
            throw;
        }
        try {
            relocate_into(new_data);
        } catch (...) {
            std::destroy_at(new_data + size_);
            deallocate(new_data);
            throw;
        }
        replace_buffer(new_data, new_capacity);
        ++size_;
    }

    // --- Köçürmə (relocation) -----------------------------------------------

    // Mövcud elementləri [data_, data_ + size_) yeni buffer-in əvvəlinə daşıyır.
    //
    // std::move_if_noexcept: T-nin move constructor-u noexcept-dirsə, KÖÇÜRÜR;
    // deyilsə (və kopyalana bilirsə), KOPYALAYIR. Səbəb: köçürmə zamanı
    // exception olsa, köhnə elementlərin bir hissəsi artıq "boşaldılmış" olardı və
    // vektoru əvvəlki vəziyyətinə qaytarmaq mümkün olmazdı. Kopyalama isə
    // köhnələrə toxunmur: exception olsa, köhnə buffer olduğu kimi qalır.
    //
    // Exception olsa: özü qurduğu elementləri dağıdır və exception-u ötürür.
    // Yaddaşı qaytarmaq çağıranın işidir.
    void relocate_into(T* new_data) {
        size_type built = 0;
        try {
            for (; built < size_; ++built) {
                std::construct_at(new_data + built, std::move_if_noexcept(data_[built]));
            }
        } catch (...) {
            std::destroy_n(new_data, built);
            throw;
        }
    }

    // Köhnə buffer-i (obyektləri və yaddaşı) atıb yenisinə keçir.
    // Yeni buffer artıq tam hazır olmalıdır. Exception atmır.
    void replace_buffer(T* new_data, size_type new_capacity) noexcept {
        std::destroy_n(data_, size_);
        deallocate(data_);
        data_ = new_data;
        capacity_ = new_capacity;
    }

    // capacity-lik yeni buffer alır və [first, first + count) elementlərinin
    // KOPYALARINI qurur (copy constructor üçün). Exception olsa, heç nə sızmır.
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

    size_type next_capacity() const {
        return (capacity_ == 0) ? 1 : capacity_ * 2;
    }

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

    T* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
};

}  // namespace mystl
