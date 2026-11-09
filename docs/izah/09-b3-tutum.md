# B3: Tutum (capacity)

**Fayllar:** [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp), [`tests/test_vector_capacity.cpp`](../../tests/test_vector_capacity.cpp)

## 1. Əlavə olunanlar

| Metod | Nə edir | Mürəkkəblik |
|---|---|---|
| `empty()` | `size() == 0` | O(1) |
| `max_size()` | nəzəri maksimum element sayı | O(1) |
| `reserve(n)` | (A4-dən) indi `n > max_size()` üçün `std::length_error` atır | O(size) |
| `shrink_to_fit()` | `capacity`-ni `size`-a endirir | O(size) |
| `resize(n)` | ölçünü `n` edir, yeni elementlər `T{}` olur | O(\|n − size\|) + böyümə |
| `resize(n, value)` | eyni, yeni elementlər `value`-nun kopyasıdır | eyni |

## 2. `empty()` və `[[nodiscard]]`

```cpp
[[nodiscard]] bool empty() const noexcept { return size_ == 0; }
```

`empty` sözü həm sual (*"boşdurmu?"*), həm də əmr (*"boşalt!"*) kimi oxuna bilir. `v.empty();` yazan proqramçı çox vaxt `clear()` nəzərdə tutur. `[[nodiscard]]` nəticə atılanda kompilyatorun **warning** verməsini təmin edir. `std::vector::empty` də C++20-dən `[[nodiscard]]`-dur.

## 3. `max_size()`

```cpp
size_type max_size() const noexcept {
    return static_cast<size_type>(std::numeric_limits<difference_type>::max()) / sizeof(T);
}
```

**Niyə `size_t`-nin maksimumu yox, `ptrdiff_t`-ninki?** İki iterator-un fərqi (`end() - begin()`) `difference_type`, yəni **işarəli** `std::ptrdiff_t` tipindədir. Onun maksimumu `size_t`-nin təxminən yarısıdır. Vektor bundan böyük olsa, `end() - begin()` daşar və undefined behavior olar. Ona görə bayt sayı `PTRDIFF_MAX`-dan böyük ola bilməz. `libstdc++` də eyni hesabdan istifadə edir. Test: `vector<char>` üçün `PTRDIFF_MAX`, `vector<uint64_t>` üçün `PTRDIFF_MAX / 8`.

Praktikada bu rəqəm əlçatmazdır: 64-bit sistemdə təxminən 9 eksabaytdır. Amma kod **overflow-a qarşı** qorunur: `reserve(max_size() + 1)` `std::length_error` atır və vektor dəyişmir. A4-də bu yoxlamanı `allocate` edirdi və `std::bad_array_new_length` atırdı. İndi `std::vector` kimi `std::length_error` atılır, köhnə test də buna uyğun yeniləndi.

## 4. Böyümə qərarı bir yerdə: `recommend_capacity`

A5-ə qədər `next_capacity()` sadəcə `capacity * 2` qaytarırdı. İndi `resize(100)` kimi birdən çox element tələb oluna bilir, ona görə qərar ümumiləşdirildi:

```cpp
size_type recommend_capacity(size_type required) const {
    if (required > max_size()) throw std::length_error(...);
    if (capacity_ >= max_size() / 2) return max_size();   // 2× daşardı
    return std::max(capacity_ * 2, required);
}
```

| Hal | Nəticə | Nümunə |
|---|---|---|
| tələb 2×-dən kiçikdir | **2×** (amortized O(1) qorunur) | cap 4, `resize(5)` → 8 |
| tələb 2×-dən böyükdür | **dəqiq tələb** (artıq yer ayırmırıq) | cap 8, `resize(100)` → 100 |
| `capacity == 0` | `max(0, required)` = `required` | ilk `push_back` → 1 |

`push_back` ardıcıllığı dəyişmədi (1, 2, 4, 8, ...). Köhnə testlər bunu təsdiqləyir. B6-da bu funksiya **growth policy** parametrinə çevriləcək (2× və 1.5×).

## 5. `shrink_to_fit()`

```cpp
void shrink_to_fit() {
    if (size_ == capacity_) return;            // artıq dəqiqdir
    if (size_ == 0) { deallocate(data_); data_ = nullptr; capacity_ = 0; return; }
    T* new_data = allocate(size_);
    try { relocate_into(new_data); } catch (...) { deallocate(new_data); throw; }
    replace_buffer(new_data, size_);
}
```

- Yaddaşı **yerində kiçiltmək olmur**: C++-da ayrılmış blokun ölçüsünü azaltmaq üçün standart vasitə yoxdur (`realloc` yalnız C-də var və obyektlərin constructor-larını bilmir). Ona görə yeni, kiçik buffer ayrılır və elementlər **köçürülür** (`move_if_noexcept`). Bu, O(n)-dir.
- Boş vektor yaddaşı tam qaytarır: `data() == nullptr`, invariant 2 qorunur.
- Standarta görə `shrink_to_fit` yalnız **xahişdir** (non-binding request). Bizim versiya həmişə yerinə yetirir.
- **İterator-lar etibarsız olur**, çünki buffer dəyişir (`size == capacity` halı istisnadır).

## 6. `resize`

```cpp
void resize(size_type count)                 { resize_impl(count, [](T* p) { std::construct_at(p); }); }
void resize(size_type count, const T& value) { resize_impl(count, [&value](T* p) { std::construct_at(p, value); }); }
```

İki versiya yalnız yeni elementin **necə qurulduğu** ilə fərqlənir. Bu fərq **lambda** ilə ötürülür, qalan məntiq `resize_impl`-də bir dəfə yazılıb:

```
count < size      →  [ e0 | e1 | e2 | e3 | e4 ]   resize(2)
                     [ e0 | e1 ] ░░   ░░   ░░      e2, e3, e4 dağıdılır; capacity qalır

count <= capacity →  [ e0 | e1 ] ░░ ░░ ░░          resize(4)
                     [ e0 | e1 | n  | n  ] ░░      yerində qurulur

count > capacity  →  yeni buffer: əvvəl yeni elementlər, sonra köhnələr köçürülür
```

### Exception safety (strong) — üç yolun hər biri

| Yol | Exception olsa nə edilir |
|---|---|
| kiçilmə | destructor-lar exception atmır, problem yoxdur |
| yerində böyümə | bu çağırışda qurulan yeni elementlər dağıdılır, `size_` köhnə dəyərinə qaytarılır |
| yenidən ayırma | `reallocate_with_tail`: yeni buffer tam hazır olana qədər köhnəyə toxunulmur |

Testlər ikisini də `Tracked` ilə yoxlayır: exception-dan sonra `size`, `capacity` və `live()` əvvəlki kimidir.

### `reallocate_with_tail`: ortaq yenidən ayırma

`push_back` (A4–A5) və `resize` eyni işi görür: yeni buffer, **sona n yeni element**, köhnələrin köçürülməsi. A5-də bu kod `append`-in içində idi. İndi bir funksiyadadır:

```cpp
template <typename ConstructOne>
void reallocate_with_tail(size_type new_capacity, size_type n, ConstructOne construct_tail);
```

1. Yeni buffer ayır.
2. `[size_, size_ + n)`-də **əvvəlcə** yeni elementləri qur. Alias halı da belə həll olunur: `v.resize(10, v[0])`-da `v[0]` hələ canlıdır.
3. Köhnələri köçür (`relocate_into`).
4. Köhnə buffer-i at (`replace_buffer`, `noexcept`).

Hər addımın öz `catch`-i var. Riskli işlər əvvəl görülür, geri dönüşü olmayan iş sonda.

**Lambda-lar və template-lər:** `ConstructOne` lambda-nın tipidir. Template olduğu üçün kompilyator lambda-nı birbaşa koda yerləşdirə bilir (inline), funksiya pointer-indən fərqli olaraq əlavə xərc yoxdur.

## 7. Testlər

| Test | Nəyi yoxlayır |
|---|---|
| `empty` | boş, əlavədən sonra, `pop_back`-dən sonra |
| `max_size` | `char` və `uint64_t` üçün dəqiq dəyər |
| `reserve` > `max_size` | `length_error`, vektor dəyişmir |
| `shrink_to_fit` | `capacity == size`; boş vektor yaddaşı qaytarır və istifadəyə yararlı qalır; dəqiq vektor yenidən ayrılmır; köçürür, kopyalamır |
| `resize` böyümə / kiçilmə | sıfırlar, dağıdılan obyektlərin sayı, `capacity` qalır |
| `resize` eyni ölçü, `resize(0)` | heç nə dəyişmir; boşalır, amma `capacity` qalır |
| `resize(n, value)` | kopyalar; `resize(5, v[0])` yenidən ayırma ilə (alias) |
| böyümə siyasəti | 4 → `resize(5)` → 8, `resize(100)` → 100 |
| exception (yerində və yenidən ayırma ilə) | vektor və sayğaclar dəyişmir |

**Nəticə:** 115 test, 2671 yoxlama, hamısı keçir; build-də warning yoxdur.

## 8. Müdafiədə gələ biləcək suallar

**`resize` ilə `reserve`-in fərqi?**
`reserve` yalnız **yer** ayırır: `size` dəyişmir, heç bir obyekt qurulmur. `resize` **elementlərin sayını** dəyişir: obyektlər qurulur və ya dağıdılır, lazım olsa yer də ayrılır.

**`resize` kiçildəndə yaddaş qaytarılırmı?**
Xeyr, yalnız elementlər dağıdılır, `capacity` qalır. Yaddaşı qaytarmaq üçün `shrink_to_fit()` çağırılmalıdır.

**`shrink_to_fit` niyə O(n)-dir?**
Ayrılmış yaddaşı yerində kiçiltmək mümkün deyil. Yeni kiçik buffer ayrılır və bütün elementlər oraya köçürülür.

**`max_size()` niyə `SIZE_MAX / sizeof(T)` deyil?**
İki iterator-un fərqi işarəli `ptrdiff_t`-dir. Buffer `PTRDIFF_MAX` baytdan böyük olsa, `end() - begin()` daşardı.

**`resize(n)` exception atsa, vektor nə vəziyyətdə qalır?**
Əvvəlki vəziyyətdə (strong guarantee). Yerində böyümədə yeni elementlər dağıdılır və `size` bərpa olunur. Yenidən ayırmada isə köhnə buffer-ə heç toxunulmur.
