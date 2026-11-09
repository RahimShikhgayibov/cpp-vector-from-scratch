# A1: v1 — `IntVector`

**Fayllar (A1 commit-indəki vəziyyət):** [`include/mystl/int_vector.hpp`](https://github.com/RahimShikhgayibov/cpp-vector-from-scratch/blob/2ae0fe248dc8ec80d6ffdf43f9afc9953fad5baf/include/mystl/int_vector.hpp), [`tests/test_int_vector.cpp`](https://github.com/RahimShikhgayibov/cpp-vector-from-scratch/blob/2ae0fe248dc8ec80d6ffdf43f9afc9953fad5baf/tests/test_int_vector.cpp)

> A3-də bu fayllar `vector.hpp` və `test_vector.cpp` adı ilə template-ə çevrildi. Linklər v1-in olduğu commit-ə aparır.

## 1. Problem

Adi massivin ölçüsü sabitdir: `int a[100];`. Neçə element lazım olacağını əvvəlcədən bilmiriksə, **böyüyə bilən** massiv lazımdır. Bu, dinamik massivdir, yəni vektorun ən sadə forması.

## 2. Yaddaş modeli: `size` və `capacity`

```
size = 3, capacity = 4

data_ ──► [ 10 | 20 | 30 | ?? ]
            0    1    2    3
           └── size_ ──┘
           └──── capacity_ ────┘
```

- **`size_`**: neçə element **həqiqətən** var.
- **`capacity_`**: ayrılmış yer **neçə elementə** çatır.
- **`data_`**: heap-də ayrılmış massivin başlanğıcına pointer.

Bu ikisini ayırmağımızın səbəbi odur ki, hər `push_back`-də yenidən yaddaş ayırmaq çox bahalıdır. Əvvəlcədən artıq yer alırıq, dolana qədər yeni yaddaşa ehtiyac qalmır.

### İnvariantlar

İnvariant, sinifin **hər public metoddan əvvəl və sonra** doğru olan qaydalarıdır:

1. `size_ <= capacity_`
2. `capacity_ == 0` olarsa, `data_ == nullptr`
3. `data_[0 .. size_-1]` istifadəçinin əlavə etdiyi elementlərdir

Hər metodu yazanda "bu metod invariantları pozmur?" deyə soruşun. Testlər də bunu yoxlayır (`size never exceeds capacity`).

## 3. Kod hissə-hissə

### Üzv dəyişənlər

```cpp
int* data_ = nullptr;
std::size_t size_ = 0;
std::size_t capacity_ = 0;
```

- `= nullptr`, `= 0`: **default member initializer**. Hər constructor bu dəyərlərlə başlayır. Ona görə `IntVector() = default;` yazmaq kifayətdir.
- **`std::size_t` niyə, `int` yox?** `size_t` işarəsiz (unsigned) tipdir və yaddaşdakı istənilən massivin ölçüsünü saxlaya biləcək qədər böyükdür (64-bit sistemdə 64 bit). Mənfi ölçü olmur. `std::vector::size()` də `size_t` qaytarır.
- Sonundakı `_`: üzv dəyişənlərin adlandırma qaydasıdır. Lokal dəyişənlərlə qarışmır.

### Destructor

```cpp
~IntVector() { delete[] data_; }
```

Obyekt dağıdılanda heap yaddaşını qaytarır. **`delete[] nullptr` təhlükəsizdir**, heç nə etmir. Ona görə boş vektor üçün ayrıca `if` yazmırıq.

### `push_back`

```cpp
void push_back(int value) {
    if (size_ == capacity_) {
        grow();
    }
    data_[size_] = value;
    ++size_;
}
```

1. Yer qalmayıbsa, böyüt.
2. Elementi ilk boş yerə (`size_` indeksinə) yaz.
3. `size_`-i artır.

### `grow` — ürək hissəsi

```cpp
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
```

- **`capacity_ == 0` yoxlamasının səbəbi:** `0 * 2 = 0`-dır. Yoxlama olmasa, boş vektor heç vaxt böyüməz və `data_[0]`-a yazmaq nullptr-ə yazmaq olar.
- **Sıranın əhəmiyyəti:** əvvəl köçürürük, **sonra** silirik. Əvvəl silsək, köçürüləcək məlumat itər.
- **Gizli üstünlük:** `new` yaddaş tapmasa, `std::bad_alloc` exception atır. Biz isə əvvəlcə yeni yeri ayırırıq və köhnəyə toxunmuruq. Ona görə xəta olsa, vektor köhnə vəziyyətində qalır. Bu, **strong exception guarantee**-nin ilkin formasıdır (B6-da tam işlənir).

### `operator[]` — iki versiya

```cpp
int&       operator[](std::size_t index)       { return data_[index]; }
const int& operator[](std::size_t index) const { return data_[index]; }
```

- **Reference (`int&`) qaytarır.** Ona görə `v[0] = 42;` işləyir. Dəyər qaytarsaydı, kopyanı dəyişərdik.
- **`const` versiya:** `const IntVector&` ilə yalnız `const` metodları çağırmaq olur. Bu versiya olmasa, const vektordan oxumaq mümkün olmazdı. Ona görə `const int&` qaytarır: oxumaq olar, dəyişmək olmaz.
- **Sərhəd yoxlanmır.** `std::vector::operator[]` də yoxlamır, çünki sürət vacibdir. Sərhədi yoxlayan, `std::out_of_range` atan `at()` B1-də yazılacaq.

### `size()` və `capacity()`

```cpp
std::size_t size() const { return size_; }
```

Sondakı `const` "bu metod obyekti dəyişmir" deməkdir. Kompilyator bunu yoxlayır: içəridə `size_ = 5;` yazsanız, xəta verər.

## 4. Niyə 2 dəfə? Amortized O(1)

Fərz edək ki, `n = 2^k` dəfə `push_back` edirik. Köçürmələr yalnız `grow()` zamanı olur:

```
capacity: 1 → 2 → 4 → 8 → ... → 2^k
köçürülən: 1 + 2 + 4 + ... + 2^(k-1) = 2^k − 1 < n
```

Yəni **n əməliyyatda cəmi n-dən az köçürmə** var. Bütün yazmalarla birlikdə ümumi iş təxminən `2n`-dir. Əməliyyat başına bu, sabitdir: **amortized O(1)**.

**"Amortized" nə deməkdir?** Ayrı-ayrı `push_back`-lərin bəziləri bahalıdır: grow olanda O(n). Amma bahalı olanlar o qədər seyrəkdir ki, **ortalama** bir əməliyyat O(1) olur. Hər `push_back` O(1) deyil, uzun ardıcıllığın orta qiyməti O(1)-dir.

**+1 böyüsək nə olar?** Hər push_back-də grow olur:

```
köçürülən: 0 + 1 + 2 + ... + (n−1) = n(n−1)/2  →  O(n²)
```

**+10 böyüsək nə olar?** 10 dəfə az grow olur, amma yenə `≈ n²/20`, yəni **O(n²)**. Əsas olan **vurmaqdır** (həndəsi böyümə), əlavə etmək yox.

Riyazi isbat (aggregate və ya accounting metodu) hesabatda olmalıdır: Cormen, "Amortized Analysis", dynamic tables. Benchmark-da (Faza D, sual 1) bunu ölçəcəyik.

## 5. Testlər nəyi yoxlayır?

| Test | Nəyi tutur |
|---|---|
| `new vector is empty` | Default constructor düzgün başlanğıc vəziyyəti verir |
| `push_back stores values in order` | Əsas məntiq, sıra |
| `operator[] can modify an element` | Reference qaytarılır, kopya yox |
| `const object can be read` | `const` versiyalar mövcuddur və işləyir |
| `capacity doubles 1, 2, 4, 8, ...` | Böyümə siyasəti dəqiq 2× və 0→1 halı |
| `size never exceeds capacity` | İnvariant 1 hər addımda |
| `1 000 000 push_back are all correct` | Yol xəritəsinin "hazırdır" şərti; çoxlu grow-dan sonra məlumat itmir |

**1 000 000 testində niyə `first_wrong`?** Hər element üçün `CHECK` yazsaq, doctest 1 000 000 yoxlama qeyd edərdi. Bu yavaşdır, səhv olanda isə çıxış nəhəng olur. Ona görə əvvəlcə ilk səhv indeksi tapırıq, sonra bir `CHECK` edirik. Səhv olsa, mesaj dəqiq harada olduğunu göstərəcək.

**`capacity == 1u << 20` niyə?** `1 << 20 = 2^20 = 1 048 576`. Bu, 1 000 000-dan böyük olan ilk 2-nin qüvvətidir. Böyümə siyasəti başqa olsa, bu test səhvi tutar.

## 6. v1-in bilinən problemləri (növbəti addımlar)

| Problem | Nə baş verir | Həlli |
|---|---|---|
| Copy constructor yoxdur | `IntVector b = a;` pointer-i kopyalayır, iki destructor eyni yaddaşı silir: **double free** | A2: rule of three |
| Yalnız `int` | `string`, `double` saxlamaq olmur | A3: template |
| `new int[]` bütün yeri init edir | `T` üçün lazımsız obyektlər, default ctor tələbi | A4: xam yaddaş + placement new |
| Grow zamanı kopyalayır | `string` kimi tiplər üçün bahalıdır | A5: move semantics |

## 7. Müdafiədə gələ biləcək suallar

**`size` və `capacity`-nin fərqi nədir?** `size` saxlanan elementlərin sayıdır, `capacity` ayrılmış yerin həcmidir. Həmişə `size <= capacity`.

**Amortized O(1) nə deməkdir və niyə hər `push_back` O(1) deyil?** Grow olanda bütün elementlər köçürülür, bu O(n)-dir. Amma 2× böyümədə n əməliyyatda cəmi köçürmə n-dən azdır. Ortalama əməliyyat başına sabit iş düşür.

**Capacity-ni niyə 2 dəfə artırırıq, 1-1 artırsaq nə olar?** 1-1 artanda hər push_back köçürmə edir və ümumi iş O(n²) olur. Həndəsi artım isə O(n) verir.

**`operator[]` niyə reference qaytarır?** Elementi yerində dəyişmək üçün (`v[i] = x`). `const` versiyası isə const obyektdən oxumağa imkan verir.

**Grow zamanı yaddaş tapılmasa nə olur?** `new` `std::bad_alloc` atır. Yeni yeri köhnəyə toxunmadan ayırdığımız üçün vektor dəyişmədən qalır.
