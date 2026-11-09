# B6: Peşəkar səviyyə

**Fayllar:** [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp), [`tests/test_vector_professional.cpp`](../../tests/test_vector_professional.cpp), [`README.md`](../../README.md) (iterator invalidation cədvəli)

Yol xəritəsinin B6 tələbləri:

| Tələb | Harada |
|---|---|
| Hər metodun `const` versiyası | B1–B5-də yazılıb, burada hamısı bir testdə yoxlanılır |
| `push_back` və `reserve` üçün strong exception guarantee | A4-dən var, burada **hər mümkün exception nöqtəsi** üçün yoxlanılır |
| Growth factor parametr kimi (2× və 1.5×) | `growth_policy` template parametri |
| Iterator invalidation cədvəli README-də | README-yə əlavə olundu, müşahidə oluna bilən hissəsi test olunur |

## 1. Growth policy

```cpp
template <typename T, growth_policy Growth = growth::doubling>
class vector { ... };

mystl::vector<int> a;                                     // 2× (default)
mystl::vector<int, mystl::growth::one_and_half> b;        // 1.5×
```

```cpp
namespace growth {
struct doubling     { static constexpr std::size_t next(std::size_t c) noexcept { return c * 2; } };
struct one_and_half { static constexpr std::size_t next(std::size_t c) noexcept { return c + c / 2; } };
}

template <typename P>
concept growth_policy = requires(std::size_t c) {
    { P::next(c) } -> std::convertible_to<std::size_t>;
};
```

**Policy-based design:** davranışın bir hissəsi (burada böyümə qaydası) ayrıca kiçik tipə çıxarılır və template parametri kimi verilir. Seçim **kompilyasiya vaxtı** olur: `Growth::next` birbaşa koda yerləşdirilir (inline), işləmə vaxtında heç bir əlavə xərc yoxdur. `std::vector`-un allocator parametri də eyni ideyadır.

**Default arqument** sayəsində `mystl::vector<int>` yazılışı dəyişmədi. Bütün köhnə testlər dəyişmədən keçir.

**Konsept** səhv tipi dərhal tutur: `next` funksiyası olmayan tip verilsə, aydın xəta çıxır.

**Trade-off:** fərqli siyasətli vektorlar **fərqli tiplərdir**. `vector<int, doubling>`-i `vector<int, one_and_half>`-ə mənimsətmək olmur. Benchmark üçün bu, problem deyil.

### Ardıcıllıqlar (testlə yoxlanılıb)

| Siyasət | 64 `push_back` ərzində capacity |
|---|---|
| 2× | 1, 2, 4, 8, 16, 32, 64 |
| 1.5× | 1, 2, 3, 4, 6, 9, 13, 19, 28, 42, 63, 94 |

1.5×-də `1 + 1/2 = 1` (tam bölmə). `recommend_capacity` isə `max(next(c), required)` götürür, ona görə 1 → 2 olur və vektor "ilişib qalmır".

### Hansı yaxşıdır? Nəzəriyyə

| | 2× | 1.5× |
|---|---|---|
| Ümumi köçürmə (n `push_back`) | `< 2n` | `< 3n` |
| Ən pis boş yer (capacity − size) | ~50% | ~33% |
| Köhnə blokların təkrar istifadəsi | **mümkün deyil** | **mümkündür** |

**Ölçülmüş köçürmə sayı** (`Tracked` sayğacı, n = 10 000):

```
relocations for n = 10000:  2x = 16383,  1.5x = 24284
```

Hər ikisi xəttidir (amortized O(1)), 1.5× isə təxminən 1.5 dəfə çox köçürür.

**Yaddaşın təkrar istifadəsi.** 2× ilə hər yeni blok **əvvəlki bütün blokların cəmindən böyükdür**: 1 + 2 + 4 = 7 < 8. Allokator köhnə blokları birləşdirib yenisi üçün istifadə edə bilmir. Faktor qızıl nisbətdən (φ ≈ 1.618) kiçik olanda isə bir neçə addımdan sonra köhnə blokların cəmi yeni blokdan böyük olur və yaddaş təkrar istifadə oluna bilir. Bu, MSVC və Facebook `folly::fbvector`-un 1.5× seçməsinin əsas arqumentidir. `libstdc++` və `libc++` isə 2× istifadə edir: az köçürmə, sadə hesab.

Hansının **praktikada** sürətli olduğunu Faza D-nin 2-ci sualı ölçəcək: vaxt, yenidən ayırma sayı və boş qalan yaddaş.

### A1-dəki izahın dəqiqləşdirilməsi

A1-də yazılmışdı: *"n əməliyyatda cəmi n-dən az köçürmə"*. Bu, yalnız `n` 2-nin qüvvəti olanda doğrudur. Ölçmə ilk dəfə `n`-dən az olmadığını göstərdi: `n = 10 000` üçün `16 383`. Düzgün hədd **`2n`-dir**: son capacity `C < 2n` və köçürmələr `C/2 + C/4 + ... < C`. A1-in izahı düzəldildi. Bu, testin həm də nəzəriyyəni yoxladığına yaxşı nümunədir.

## 2. Strong guarantee: hər nöqtədə

A4–A5-də strong guarantee **bir neçə seçilmiş** exception nöqtəsi üçün yoxlanırdı. B6-da **bütün** nöqtələr yoxlanılır:

```cpp
for (int size = 0; size <= 16; ++size) {      // hər ölçü
    for (int k = 0; k <= size; ++k) {         // hər mümkün exception nöqtəsi
        // dolu vektor: push_back böyüdəcək və size + 1 kopya edəcək
        TrackedThrowingMove::throw_on_copy_after = k;
        v.push_back(item);   // k-cı kopya exception atır
        // vektor tam əvvəlki kimidir: size, capacity, dəyərlər, live()
    }
}
```

`push_back` (lvalue, dolu vektor) `size + 1` kopya edir: yeni element və köhnələr (`TrackedThrowingMove`-un move-u `noexcept` deyil, ona görə köhnələr kopyalanır). Exception hər kopyada bir-bir yaradılır. Bu, **153 ssenari** edir. `reserve` üçün də 136 ssenari var. Hər birində yoxlanılır:
- `size` və `capacity` dəyişməyib,
- bütün dəyərlər yerindədir,
- `live()` əvvəlki kimidir, yəni sızma və ikiqat dağıdılma yoxdur.

Bu üsul **exhaustive exception testing** adlanır. `INFO("size = ..., throw at copy #...")` xəta olsa, hansı ssenarinin uğursuz olduğunu göstərir.

## 3. `const` versiyalar

```cpp
int read_everything(const mystl::vector<int>& v) {
    // [], at, front, back, data, begin/end, cbegin/cend, rbegin/rend, crbegin/crend,
    // size, capacity, empty, max_size, ==, <
}
```

`const` reference ilə bütün oxuma əməliyyatları çağırılır. Hər hansı birinin `const` versiyası unudulsaydı, **kompilyasiya olunmazdı**. Yəni test əsasən kompilyasiya zamanı işləyir, dəyərin yoxlanması isə əlavədir.

## 4. Iterator invalidation

Iterator, pointer və reference yaddaş ünvanıdır. Ünvandakı element yerini dəyişəndə və ya buffer silinəndə onlar **etibarsız olur**. Etibarsız iterator-dan istifadə undefined behavior-dur. Tam cədvəl [README-dədir](../../README.md#iterator-invalidation). Qısaca:

| Əməliyyat | Yenidən ayırma olsa | Olmasa |
|---|---|---|
| `push_back`, `emplace_back` | **hamısı** | yalnız `end()` |
| `insert`, `emplace` | **hamısı** | `pos` və sonrakılar |
| `erase` | — | `pos` və sonrakılar |
| `pop_back` | — | son element və `end()` |
| `reserve`, `shrink_to_fit` | **hamısı** (capacity dəyişsə) | heç biri |
| `swap`, move | etibarlı qalır, amma artıq **digər** vektorun elementlərini göstərir | |

**Yenidən ayırma nə vaxt olur?** `size() == capacity()` olanda əlavə etmək. Ona görə `reserve` iterator-ları qorumaq üçün də istifadə olunur: əvvəlcədən yer ayırsan, sonrakı əlavələr buffer-i dəyişmir.

Testlər müşahidə oluna bilən qaydaları yoxlayır:
- `reserve` edilmiş vektorda `push_back` ünvanları dəyişmir;
- dolu vektorda `push_back` `data()`-nı dəyişir;
- `erase` və yenidən ayırmasız `insert`-dən sonra `pos`-dan əvvəlki iterator-lar eyni elementi göstərir;
- `swap` və move-dan sonra iterator digər vektorun elementini göstərir.

**Etibarsızlığı test etmək olmur**, çünki etibarsız iterator-dan oxumaq özü undefined behavior-dur. Linux-da ASan belə istifadəni `heap-use-after-free` kimi tutar.

## 5. Faza B yekunu

`std::vector` interfeysi (allocator və `vector<bool>` xaric) tam yazılıb:

| Qrup | Metodlar |
|---|---|
| Üzv tipləri | `value_type`, `size_type`, `difference_type`, `reference`, `const_reference`, `pointer`, `const_pointer`, `iterator`, `const_iterator`, `reverse_iterator`, `const_reverse_iterator` |
| Constructor-lar | default, `(n)`, `(n, value)`, `(first, last)`, `(initializer_list)`, copy, move; destructor |
| Mənimsəmə | `operator=` (copy, move, `initializer_list`), `assign` (3 forma) |
| Giriş | `at`, `operator[]`, `front`, `back`, `data` |
| Iterator-lar | `begin`, `end`, `cbegin`, `cend`, `rbegin`, `rend`, `crbegin`, `crend` |
| Tutum | `empty`, `size`, `max_size`, `reserve`, `capacity`, `shrink_to_fit` |
| Modifikatorlar | `clear`, `insert` (5 forma), `emplace`, `erase` (2 forma), `push_back` (2), `emplace_back`, `pop_back`, `resize` (2), `swap` |
| Sərbəst funksiyalar | `==`, `<=>` (və avtomatik `!=`, `<`, `<=`, `>`, `>=`), `swap(a, b)`, `erase`, `erase_if` |
| Əlavə | deduction guide (CTAD), growth policy |

**Bilərəkdən əhatə xaricində:** custom allocator dəstəyi və `vector<bool>` (yol xəritəsinə uyğun olaraq). Hesabatda belə qeyd olunmalıdır.

**`std::vector`-dan bilinən fərqlər** (hesabatın "trade-off-lar" bölməsi üçün):
- `assign` həmişə yeni buffer ayırır, köhnəni təkrar istifadə etmir. Bunun müqabilində strong guarantee verir.
- Aralıq `insert`-i `std::rotate` istifadə edir: sadə və alias-a qarşı təhlükəsizdir, amma bir az çox köçürmə edir.
- Bütün iterator-lar adi pointer-dir: debug yoxlamaları yoxdur.

**Nəticə:** 162 test, 10 889 yoxlama, hamısı keçir; build-də warning yoxdur.

## 6. Müdafiədə gələ biləcək suallar

**Capacity niyə 2 dəfə artır? 1.5 dəfə olsa nə dəyişər?**
İkisi də həndəsi böyümədir və amortized O(1) verir. 2× daha az köçürür (`< 2n`), amma daha çox boş yer saxlayır (50%-ə qədər) və köhnə blokları təkrar istifadə etmək mümkün olmur. 1.5× daha çox köçürür (`< 3n`), amma yaddaşa qənaət edir və qızıl nisbətdən kiçik olduğu üçün köhnə blokların təkrar istifadəsi mümkündür. Bizdə ikisi də parametr kimi var. Ölçülmüş köçürmə sayı n = 10 000 üçün: 16 383 və 24 284.

**Hansı əməliyyatlar iterator-ları etibarsız edir?**
Buffer-i dəyişən hər əməliyyat hamısını etibarsız edir: dolu vektora əlavə etmək, `reserve`, `shrink_to_fit`. `insert` və `erase` yenidən ayırma olmasa da mövqedən sonrakıları etibarsız edir. Tam cədvəl README-dədir.

**Strong guarantee-ni necə sübut edirsiniz?**
Sayğaclı test tipi ilə exception-u hər mümkün kopyada bir-bir yaradırıq: `push_back` üçün 153, `reserve` üçün 136 ssenari. Hər ssenaridən sonra ölçü, capacity, dəyərlər və canlı obyekt sayı əvvəlki kimidir.

**Policy-based design nədir?**
Sinfin dəyişə bilən davranışını ayrıca tipə çıxarıb template parametri kimi vermək. Seçim kompilyasiya vaxtı edilir və işləmə vaxtında xərc yoxdur. Bizdə böyümə qaydası belədir, `std::vector`-da allocator.
