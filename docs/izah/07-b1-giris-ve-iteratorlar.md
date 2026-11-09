# B1: Elementə giriş və iterator-lar

**Fayllar:** [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp), [`tests/test_vector_access.cpp`](../../tests/test_vector_access.cpp)

Faza B-də vektorun yaddaş modeli artıq dəyişmir (Faza A-da quruldu). İndi `std::vector`-un interfeysini tamamlayırıq. Hər metodun dəqiq davranışı cppreference-dəki `std::vector` səhifəsi ilə yoxlanılıb.

## 1. Əlavə olunanlar

| Metod | Nə edir | Mürəkkəblik |
|---|---|---|
| `at(i)` | `i`-ci element, sərhəd yoxlanır, `std::out_of_range` atır | O(1) |
| `front()`, `back()` | ilk / son element | O(1) |
| `data()` | buffer-in başlanğıcına pointer | O(1) |
| `begin()`, `end()` | iterator-lar | O(1) |
| `cbegin()`, `cend()` | həmişə `const` iterator-lar | O(1) |
| `rbegin()`, `rend()`, `crbegin()`, `crend()` | tərs iterator-lar | O(1) |

Hər metodun `const` versiyası var: `const` vektordan yalnız oxumaq olar.

Tip adları: `difference_type`, `pointer`, `const_pointer`, `iterator`, `const_iterator`, `reverse_iterator`, `const_reverse_iterator`.

## 2. `operator[]` və `at()`: narrow və wide contract

```cpp
reference operator[](size_type index) { return data_[index]; }

reference at(size_type index) {
    check_index(index);            // index >= size_  →  std::out_of_range
    return data_[index];
}
```

| | `operator[]` | `at()` |
|---|---|---|
| Sərhəd xaricində | **undefined behavior** | **exception** (`std::out_of_range`) |
| Müqavilə | **narrow contract**: düzgün indeks vermək **çağıranın** borcudur | **wide contract**: istənilən indeks qəbul olunur, səhv aydın şəkildə bildirilir |
| Qiyməti | heç nə | hər çağırışda bir müqayisə |

**Niyə ikisi də var?** C++-ın prinsipi belədir: *"istifadə etmədiyin şeyin pulunu ödəmirsən."* Dövrdə `for (i = 0; i < v.size(); ++i) v[i]` yazanda indeks onsuz da düzgündür, yoxlama artıqdır. İndeks kənardan gələndə (istifadəçi girişi) isə `at()` təhlükəsizdir.

Exception mesajı indeksi və ölçünü göstərir: `mystl::vector::at: index 7 >= size 2`. Bu, debug zamanı kömək edir. Test mesajda hər iki ədədin olduğunu yoxlayır.

## 3. `front()`, `back()`, `data()`

- `front()` = `data_[0]`, `back()` = `data_[size_ - 1]`. **Boş vektorda undefined behavior-dur.** `size_ - 1` `size_t` üçün `0 - 1` olur, yəni nəhəng ədəd. `std::vector`-da da yoxlama yoxdur (narrow contract).
- `data()` buffer pointer-idir. C funksiyalarına massiv ötürmək üçün istifadə olunur: `write(fd, v.data(), v.size())`. Boş vektorda `nullptr` ola bilər, bu da standartla uyğundur.

## 4. Iterator nədir?

Iterator konteynerin elementləri üzərində "gəzən" obyektdir. Bütün STL alqoritmləri (`std::sort`, `std::find`, ...) konteynerlə yox, **iterator cütü** ilə işləyir. Belə olanda bir alqoritm istənilən konteynerə tətbiq oluna bilir.

```
 begin()                         end()
   ↓                               ↓
 [ e0 | e1 | e2 | e3 ] ░░ ░░
                         ↑
               son elementdən SONRAKI yer
               (dereference etmək olmaz)
```

`[begin, end)` **yarım-açıq aralıqdır**: `begin` daxildir, `end` daxil deyil. Faydaları:
- boş aralıq: `begin == end`;
- element sayı: `end - begin`;
- dövr: `for (it = begin; it != end; ++it)`.

### Bizim iterator niyə sadəcə `T*`-dır?

```cpp
using iterator = T*;
using const_iterator = const T*;
```

Vektorun elementləri yaddaşda **ardıcıl** durur. Adi pointer iterator-dan gözlənilən hər şeyi edir: `*it`, `++it`, `--it`, `it + n`, `it[n]`, `it2 - it1`, `<`. C++20-də bu, ən güclü kateqoriyadır: **contiguous iterator**. Test bunu standart konseptlərlə yoxlayır:

```cpp
CHECK(std::contiguous_iterator<V::iterator>);
CHECK(std::ranges::contiguous_range<V>);
```

Yol xəritəsi deyir: *"əvvəl `T*` kimi, istəsən sonra öz iterator class-ın"*. Öz sinfi yazmağın faydası Debug rejimində yoxlamalar əlavə etmək ola bilər (məsələn, "bu iterator hələ etibarlıdırmı?"). Amma bu, interfeysi dəyişmir. `libstdc++` də daxildə pointer-i sadə bir sinfə bükür.

### `const_iterator` ilə `const iterator` fərqi

| Yazılış | Pointer analoqu | İterator-u dəyişmək (`++it`) | Elementi dəyişmək (`*it = x`) |
|---|---|---|---|
| `const_iterator` | `const T*` | olar | **olmaz** |
| `const iterator` | `T* const` | **olmaz** | olar |

`const` vektorun `begin()`-i `const_iterator` qaytarır. `cbegin()` isə qeyri-const vektorda da `const_iterator` qaytarır. Bu, "yalnız oxuyacağam" niyyətini açıq göstərmək üçündür.

### Tərs iterator-lar

```cpp
reverse_iterator rbegin() { return reverse_iterator(end()); }
reverse_iterator rend()   { return reverse_iterator(begin()); }
```

`std::reverse_iterator` adi iterator-u "çevirir": `++` geri gedir. Bir nüansı var: `rbegin()` daxildə `end()`-də dayanır, amma `*rbegin()` **bir əvvəlki** elementi, yəni `end() - 1`-i qaytarır. Belə olanda `[rbegin, rend)` də düzgün yarım-açıq aralıq olur:

```
           rend()                    rbegin()
             ↓                          ↓  (base)
         [ e0 | e1 | e2 | e3 ]
            ↑                   ↑
      *(rend()-1)          *rbegin() == e3
```

Test: `*v.rbegin() == 4`, `&*v.rbegin() == &v.back()`, `v.rbegin().base() == v.end()`.

## 5. Range-based for necə işləyir?

```cpp
for (int x : v) { ... }
```

Kompilyator bunu təxminən belə açır:

```cpp
for (auto it = v.begin(), e = v.end(); it != e; ++it) {
    int x = *it;
    ...
}
```

Yəni `begin()` və `end()` olan istənilən tip range-based for ilə işləyir. Vektorumuz `std::sort`, `std::accumulate`, `std::ranges::sort` və `std::views::transform` ilə də işləyir, testlərdə hamısı yoxlanılıb.

## 6. Iterator invalidation (ilkin)

Iterator yaddaş ünvanıdır. Buffer dəyişəndə **köhnə iterator-lar, pointer-lər və reference-lər etibarsız olur**:

```cpp
auto it = v.begin();
v.push_back(x);      // böyümə olsa, köhnə buffer silinir
*it;                 // undefined behavior: silinmiş yaddaş
```

Tam cədvəl B6-da README-yə əlavə olunacaq.

## 7. `noexcept`

`begin`, `end`, `data`, `size`, `capacity` `noexcept` elan olunub. Onlar sadəcə üzv qaytarır, exception mümkün deyil. `std::vector`-da da belədir. `operator[]`, `front` və `back` isə `noexcept` deyil. Standart onları bilərəkdən belə saxlayır: narrow contract funksiyaları debug rejimində yoxlama edib exception atmağa haqq qazanır.

## 8. Testlər

| Qrup | Nəyi yoxlayır |
|---|---|
| `at` | düzgün indeks, sərhəd xaricində exception (o cümlədən `-1`), boş vektor, `const` versiya, mesajın məzmunu |
| `front`, `back` | oxumaq, dəyişmək, `const`; bir elementdə ikisi eyni obyektdir |
| `data` | boş vektorda `nullptr`, `&v[0]`-a bərabərdir, `const` tipi |
| iterator konseptləri | `contiguous_iterator`, `random_access_iterator`, `contiguous_range`, `sized_range` |
| `begin`/`end` | boş vektor, `end - begin == size`, range-for ilə oxumaq və yazmaq |
| `const` iterator-lar | tiplər `decltype` ilə yoxlanılır |
| tərs iterator-lar | sıra, `base()`, `crbegin` |
| alqoritmlər | `sort`, `is_sorted`, `accumulate`, `find`, `binary_search`, `reverse`, `ranges::sort`, `views::transform` |

**Nəticə:** 79 test, 2541 yoxlama, hamısı keçir; build-də warning yoxdur.

## 9. Müdafiədə gələ biləcək suallar

**`operator[]` ilə `at()`-in fərqi nədir?**
`operator[]` sərhədi yoxlamır, səhv indeks undefined behavior-dur. `at()` yoxlayır və `std::out_of_range` atır. İndeks etibarlıdırsa `[]` sürətlidir, kənardan gəlirsə `at()` təhlükəsizdir.

**Niyə iterator kimi adi pointer istifadə olunur?**
Elementlər yaddaşda ardıcıldır, pointer isə iterator-un bütün əməliyyatlarını dəstəkləyir və C++20-nin `contiguous_iterator` konseptini ödəyir. Əlavə sinif yazmağa ehtiyac yoxdur.

**`end()` niyə son elementdən sonrakı yeri göstərir?**
Yarım-açıq aralıq `[begin, end)` boş aralığı (`begin == end`) və element sayını (`end - begin`) sadə ifadə edir. Dövr şərti də `it != end` olur.

**`const_iterator` ilə `const iterator`-un fərqi?**
`const_iterator` elementi dəyişməyə icazə vermir, amma özü hərəkət edə bilir (`const T*`). `const iterator` isə hərəkət edə bilmir, amma elementi dəyişə bilir (`T* const`).

**`*rbegin()` niyə `end() - 1` qaytarır?**
`rbegin()` daxildə `end()`-də saxlanılır və dereference bir addım geri baxır. Belə olanda `[rbegin, rend)` də yarım-açıq aralıq olur və tərs dövr adi dövrlə eyni qaydada yazılır.
