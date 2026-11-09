#pragma once

#include <algorithm>         // std::max
#include <cstddef>           // std::size_t, std::ptrdiff_t
#include <initializer_list>  // std::initializer_list
#include <iterator>   // std::reverse_iterator, std::input_iterator, std::distance
#include <limits>     // std::numeric_limits
#include <memory>     // std::construct_at, std::destroy_at, std::destroy_n, std::uninitialized_copy_n
#include <new>        // ::operator new, std::align_val_t, std::bad_array_new_length
#include <stdexcept>  // std::out_of_range, std::length_error
#include <string>     // std::to_string
#include <utility>    // std::move, std::forward, std::exchange, std::move_if_noexcept, std::swap

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
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    // Elementlər yaddaşda ardıcıl durduğu üçün adi pointer tam hüquqlu
    // iterator-dur (contiguous iterator): ++, --, +n, -n, [], müqayisə.
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

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

    // --- B2: Constructor-lar ---------------------------------------------------
    //
    // Hamısı əvvəlcə default constructor-a DELEGATE edir (": vector()").
    // Səbəb: delegate edilən constructor bitən kimi obyekt "qurulmuş" sayılır.
    // Gövdədə exception olsa, destructor ÇAĞIRILIR və artıq qurulmuş
    // elementlər dağıdılır, yaddaş qaytarılır. Delegation olmasaydı, yarımçıq
    // obyektin destructor-u çağırılmazdı və hər şey sızardı.

    // count ədəd value-initialized element: int üçün 0, string üçün "".
    // explicit: `vector<int> v = 5;` kimi gözlənilməz çevirmələrə icazə vermir.
    explicit vector(size_type count) : vector() {
        reserve(count);
        for (; size_ < count; ++size_) {
            std::construct_at(data_ + size_);  // T{} (value-initialization)
        }
    }

    // count ədəd value-nun kopyası.
    vector(size_type count, const T& value) : vector() {
        reserve(count);
        for (; size_ < count; ++size_) {
            std::construct_at(data_ + size_, value);
        }
    }

    // [first, last) aralığındakı elementlərin kopyası.
    // std::input_iterator konsepti vacibdir: onsuz vector<int>(5, 1) çağırışı
    // bu template-i InputIt = int ilə seçərdi və "5 ədəd 1" əvəzinə int-ləri
    // iterator kimi istifadə etməyə çalışardı.
    template <std::input_iterator InputIt>
    vector(InputIt first, InputIt last) : vector() {
        if constexpr (std::forward_iterator<InputIt>) {
            // Aralığı iki dəfə keçmək olar: əvvəl say, bir dəfə yer ayır.
            reserve(static_cast<size_type>(std::distance(first, last)));
            for (; first != last; ++first) {
                std::construct_at(data_ + size_, *first);
                ++size_;
            }
        } else {
            // Input iterator (məs. istream_iterator) yalnız bir dəfə keçilə bilir:
            // neçə element olacağını əvvəlcədən bilmirik, ona görə bir-bir əlavə edirik.
            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }
    }

    // vector<int> v{1, 2, 3};   vector<int> v = {1, 2, 3};
    vector(std::initializer_list<T> init) : vector(init.begin(), init.end()) {}

    // --- B2: Mənimsəmə ---------------------------------------------------------
    //
    // Hamısı "qur, sonra swap" üsulu ilə yazılıb (copy-and-swap kimi):
    // strong exception guarantee verir və value vektorun öz elementi olsa belə
    // düzgün işləyir: v.assign(3, v[0]).
    // Qiyməti: std::vector köhnə buffer-i yetərli olanda təkrar istifadə edir,
    // bizim versiya isə həmişə yeni buffer ayırır.

    vector& operator=(std::initializer_list<T> init) {
        assign(init);
        return *this;
    }

    void assign(size_type count, const T& value) {
        vector replacement(count, value);
        swap(replacement);
    }

    template <std::input_iterator InputIt>
    void assign(InputIt first, InputIt last) {
        vector replacement(first, last);
        swap(replacement);
    }

    void assign(std::initializer_list<T> init) {
        assign(init.begin(), init.end());
    }

    void swap(vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    // --- B4: Modifikatorlar -------------------------------------------------------

    // lvalue: arqument kopyalanır.  v.push_back(s);
    void push_back(const T& value) { emplace_back(value); }

    // rvalue: arqument köçürülür.    v.push_back(std::move(s));  v.push_back(T(...));
    void push_back(T&& value) { emplace_back(std::move(value)); }

    // Elementi BİRBAŞA vektorun yaddaşında, verilən arqumentlərlə qurur:
    //   v.emplace_back("abc", 3);   // T("abc", 3) — müvəqqəti obyekt yoxdur
    // Args&&... "forwarding reference" paketidir: hər arqument öz növü
    // (lvalue/rvalue) ilə T-nin constructor-una ötürülür (std::forward).
    // Yeni elementə reference qaytarır (C++17).
    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (size_ < capacity_) {
            std::construct_at(data_ + size_, std::forward<Args>(args)...);
            ++size_;
        } else {
            // Arqumentlər köhnə buffer-dəki elementə istinad edə bilər: v.emplace_back(v[0]).
            // reallocate_with_tail yeni elementi köhnə buffer dağıdılmamışdan ƏVVƏL qurur.
            reallocate_with_tail(recommend_capacity(size_ + 1), 1, [&](T* p) {
                std::construct_at(p, std::forward<Args>(args)...);
            });
        }
        return back();
    }

    // Son elementi dağıdır. Yaddaş qalır (capacity dəyişmir).
    // Boş vektorda çağırmaq undefined behavior-dur (std::vector-da da belədir).
    void pop_back() {
        --size_;
        std::destroy_at(data_ + size_);
    }

    // Bütün elementləri dağıdır. capacity qalır (yaddaş qaytarılmır).
    void clear() noexcept {
        std::destroy_n(data_, size_);
        size_ = 0;
    }

    // pos-dan ƏVVƏL yeni element qurur, ona iterator qaytarır.
    //
    //   [ a | b | c | d ]          emplace(begin() + 1, x)
    //   1. sona qur:      [ a | b | c | d | x ]     (emplace_back: alias və böyümə təhlükəsiz)
    //   2. x-i saxla:     tmp = move(x)
    //   3. sağa sür:      [ a | b | b | c | d ]     (std::move_backward)
    //   4. yerinə qoy:    [ a | x | b | c | d ]
    //
    // Mürəkkəblik: O(1) + O(end - pos) köçürmə. Sonda: amortized O(1).
    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        const size_type index = index_of(pos);
        emplace_back(std::forward<Args>(args)...);
        if (index + 1 != size_) {
            T new_element(std::move(back()));
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = std::move(new_element);
        }
        return begin() + index;
    }

    iterator insert(const_iterator pos, const T& value) { return emplace(pos, value); }
    iterator insert(const_iterator pos, T&& value) { return emplace(pos, std::move(value)); }

    // pos-dan əvvəl value-nun count kopyası.
    iterator insert(const_iterator pos, size_type count, const T& value) {
        const size_type index = index_of(pos);
        const size_type old_size = size_;
        append_n(count, [&value](T* p) { std::construct_at(p, value); });
        std::rotate(begin() + index, begin() + old_size, end());
        return begin() + index;
    }

    // pos-dan əvvəl [first, last) aralığının kopyası.
    // Şərt (std::vector-da da belədir): first və last bu vektorun iterator-ları olmamalıdır.
    template <std::input_iterator InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        const size_type index = index_of(pos);
        const size_type old_size = size_;
        if constexpr (std::forward_iterator<InputIt>) {
            const auto count = static_cast<size_type>(std::distance(first, last));
            append_n(count, [&first](T* p) {
                std::construct_at(p, *first);
                ++first;
            });
        } else {
            // Sayı bilmirik: bir-bir sona əlavə et. Uğursuz olsa, əlavə olunanları geri al.
            try {
                for (; first != last; ++first) {
                    emplace_back(*first);
                }
            } catch (...) {
                std::destroy(data_ + old_size, data_ + size_);
                size_ = old_size;
                throw;
            }
        }
        std::rotate(begin() + index, begin() + old_size, end());
        return begin() + index;
    }

    iterator insert(const_iterator pos, std::initializer_list<T> init) {
        return insert(pos, init.begin(), init.end());
    }

    // pos-dakı elementi silir. Sonrakı elementlər bir addım sola sürüşür.
    // Silinəndən sonrakı elementə iterator qaytarır (sonuncu silinibsə end()).
    // Mürəkkəblik: O(end - pos).
    iterator erase(const_iterator pos) {
        const size_type index = index_of(pos);
        std::move(begin() + index + 1, end(), begin() + index);
        pop_back();
        return begin() + index;
    }

    // [first, last) aralığını silir.
    iterator erase(const_iterator first, const_iterator last) {
        const size_type index = index_of(first);
        const auto count = static_cast<size_type>(last - first);
        if (count != 0) {
            iterator new_end = std::move(begin() + index + count, end(), begin() + index);
            std::destroy(new_end, end());
            size_ -= count;
        }
        return begin() + index;
    }

    // Ən azı new_capacity elementlik yer ayırır, mövcud elementləri köçürür.
    // Heç bir yeni obyekt QURMUR. Kiçik və ya bərabər dəyər üçün heç nə etmir.
    // new_capacity > max_size() olarsa std::length_error atır.
    void reserve(size_type new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }
        if (new_capacity > max_size()) {
            throw std::length_error("mystl::vector::reserve: capacity exceeds max_size()");
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

    // --- B1: Elementə giriş -----------------------------------------------------

    // Sərhəd yoxlanmır: index >= size() undefined behavior-dur ("narrow contract").
    reference operator[](size_type index) { return data_[index]; }
    const_reference operator[](size_type index) const { return data_[index]; }

    // Sərhəd yoxlanır: index >= size() olarsa std::out_of_range atır ("wide contract").
    reference at(size_type index) {
        check_index(index);
        return data_[index];
    }
    const_reference at(size_type index) const {
        check_index(index);
        return data_[index];
    }

    // Boş vektorda front()/back() undefined behavior-dur (std::vector-da da belədir).
    reference front() { return data_[0]; }
    const_reference front() const { return data_[0]; }
    reference back() { return data_[size_ - 1]; }
    const_reference back() const { return data_[size_ - 1]; }

    // Buffer-in başlanğıcı. C funksiyalarına massiv kimi ötürmək üçün.
    // Boş vektorda nullptr ola bilər.
    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }

    // --- B1: Iterator-lar ------------------------------------------------------

    iterator begin() noexcept { return data_; }
    const_iterator begin() const noexcept { return data_; }
    iterator end() noexcept { return data_ + size_; }
    const_iterator end() const noexcept { return data_ + size_; }

    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    // rbegin() end()-də "dayanır", amma *rbegin() son elementi (end() - 1) qaytarır.
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    // --- B3: Tutum --------------------------------------------------------------

    // [[nodiscard]]: `v.empty();` yazıb nəticəni atsan, kompilyator xəbərdarlıq edir
    // (çox vaxt səhvən clear() nəzərdə tutulur).
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }

    // Nəzəri maksimum element sayı. end() - begin() fərqi difference_type-a
    // (std::ptrdiff_t, işarəli) sığmalıdır, ona görə size_t-nin yarısı əsas götürülür.
    size_type max_size() const noexcept {
        return static_cast<size_type>(std::numeric_limits<difference_type>::max()) / sizeof(T);
    }

    // capacity-ni size-a endirir: artıq yaddaşı qaytarır.
    // Elementlər yeni, dəqiq ölçülü buffer-ə köçürülür. Exception olsa, heç nə dəyişmir.
    void shrink_to_fit() {
        if (size_ == capacity_) {
            return;
        }
        if (size_ == 0) {
            deallocate(data_);
            data_ = nullptr;
            capacity_ = 0;
            return;
        }
        T* new_data = allocate(size_);
        try {
            relocate_into(new_data);
        } catch (...) {
            deallocate(new_data);
            throw;
        }
        replace_buffer(new_data, size_);
    }

    // Ölçünü count-a dəyişir.
    //   count < size:  artıq elementlər (sondan) dağıdılır, capacity qalır
    //   count > size:  yeni elementlər sona əlavə olunur: T{} (value-initialized)
    // Exception olsa, vektor dəyişmir (strong guarantee).
    void resize(size_type count) {
        resize_impl(count, [](T* p) { std::construct_at(p); });
    }

    // Eyni, amma yeni elementlər value-nun kopyasıdır.
    // value vektorun öz elementi ola bilər: v.resize(10, v[0]).
    void resize(size_type count, const value_type& value) {
        resize_impl(count, [&value](T* p) { std::construct_at(p, value); });
    }

private:
    void check_index(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("mystl::vector::at: index " + std::to_string(index) +
                                    " >= size " + std::to_string(size_));
        }
    }

    // const_iterator -> indeks. insert/erase iterator qəbul edir, amma
    // böyümə zamanı iterator etibarsız olur; indeks isə dəyişmir.
    size_type index_of(const_iterator pos) const noexcept {
        return static_cast<size_type>(pos - cbegin());
    }

    // --- Element əlavə etmək -------------------------------------------------

    // Sona count yeni element əlavə edir; construct_one(p) p ünvanında birini qurur.
    // resize, insert(pos, n, value) və insert(pos, first, last) bunu istifadə edir.
    // Exception olsa, vektor dəyişmir (strong guarantee).
    template <typename ConstructOne>
    void append_n(size_type count, ConstructOne construct_one) {
        if (count == 0) {
            return;
        }
        if (count > max_size() - size_) {   // size_ + count daşmasın
            throw std::length_error("mystl::vector: size would exceed max_size()");
        }
        if (size_ + count > capacity_) {
            reallocate_with_tail(recommend_capacity(size_ + count), count, construct_one);
            return;
        }
        // Yer var: yeni elementləri yerində qur. Biri exception atsa,
        // bu çağırışda qurulanları dağıdıb vektoru əvvəlki ölçüsünə qaytarırıq.
        const size_type old_size = size_;
        const size_type new_size = size_ + count;
        try {
            for (; size_ < new_size; ++size_) {
                construct_one(data_ + size_);
            }
        } catch (...) {
            std::destroy(data_ + old_size, data_ + size_);
            size_ = old_size;
            throw;
        }
    }

    // resize(count) və resize(count, value)-nun ortaq kodu.
    template <typename ConstructOne>
    void resize_impl(size_type count, ConstructOne construct_one) {
        if (count <= size_) {
            std::destroy(data_ + count, data_ + size_);   // sondan artıqları at
            size_ = count;
            return;
        }
        append_n(count - size_, construct_one);
    }

    // --- Yenidən ayırma (reallocation) ----------------------------------------

    // new_capacity-lik yeni buffer qurur:
    //   [size_, size_ + n)  ->  construct_tail ilə n YENİ element (ƏVVƏL qurulur)
    //   [0, size_)          ->  köhnə elementlər köçürülür (relocate_into)
    // sonra köhnə buffer-i atır. Exception olsa, heç nə dəyişmir və sızmır.
    //
    // Yeni elementlərin əvvəl qurulması alias halını həll edir: onlar köhnə
    // buffer-dəki elementdən (v[0]) qurulursa, o element hələ canlıdır.
    template <typename ConstructOne>
    void reallocate_with_tail(size_type new_capacity, size_type n, ConstructOne construct_tail) {
        T* new_data = allocate(new_capacity);
        size_type built = 0;
        try {
            for (; built < n; ++built) {
                construct_tail(new_data + size_ + built);
            }
        } catch (...) {
            std::destroy_n(new_data + size_, built);
            deallocate(new_data);
            throw;
        }
        try {
            relocate_into(new_data);
        } catch (...) {
            std::destroy_n(new_data + size_, n);
            deallocate(new_data);
            throw;
        }
        replace_buffer(new_data, new_capacity);
        size_ += n;
    }

    // Ən azı `required` elementə yer lazımdır: yeni capacity nə olsun?
    // Həndəsi böyümə (2×), amma tələb olunandan az yox və max_size()-dan çox yox.
    size_type recommend_capacity(size_type required) const {
        const size_type max = max_size();
        if (required > max) {
            throw std::length_error("mystl::vector: size would exceed max_size()");
        }
        if (capacity_ >= max / 2) {
            return max;   // 2× daşardı
        }
        return std::max(capacity_ * 2, required);
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

// Deduction guide (CTAD): `mystl::vector v(list.begin(), list.end());` yazanda
// T-ni iterator-un element tipindən çıxarır. initializer_list üçün guide
// lazım deyil: `mystl::vector v{1, 2, 3};` constructor-dan özü çıxarılır.
template <std::input_iterator InputIt>
vector(InputIt, InputIt) -> vector<std::iter_value_t<InputIt>>;

}  // namespace mystl
