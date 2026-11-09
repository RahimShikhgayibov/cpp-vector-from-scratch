# A5: v5 — Move semantics və rule of five

**Fayllar:**
- [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp): v5
- [`experiments/a5_move_semantics.cpp`](../../experiments/a5_move_semantics.cpp): `noexcept` unudulanda nə baş verir
- [`tests/test_vector_move.cpp`](../../tests/test_vector_move.cpp): move testləri
- [`tests/test_types.hpp`](../../tests/test_types.hpp): `Tracked` (noexcept move) və `TrackedThrowingMove`

## 1. Problem

v4-ün iki zəif yeri qalmışdı:

1. **Lazımsız kopyalar.** `vector<std::string>` böyüyəndə hər sətir **kopyalanır**: yeni heap yaddaşı ayrılır, bütün simvollar köçürülür. Sonra köhnə sətir dərhal silinir. Halbuki köhnə sətrin buffer-ini sadəcə "götürmək" kifayət edərdi. `v.push_back(std::string(...))` kimi müvəqqəti obyekt verəndə də eyni şey olur: obyekt onsuz da silinəcək, amma yenə kopyalanır.
2. **Yalnız köçürülə bilən tiplər işləmir.** `std::unique_ptr<int>` kopyalana bilmir (bir yaddaşın iki sahibi ola bilməz). v4 isə hər yerdə kopyalayırdı.

## 2. lvalue və rvalue

| | Nədir | Nümunə | "Ondan götürmək olar?" |
|---|---|---|---|
| **lvalue** | Adı və ünvanı olan, sonra da istifadə olunacaq obyekt | `s`, `v[0]`, `*p` | **Yox**: sahibi hələ ondan istifadə edəcək |
| **rvalue** | Müvəqqəti obyekt, ifadə bitəndə yox olacaq | `std::string("abc")`, `f()`, `a + b` | **Bəli**: onsuz da silinəcək |

C++11 bu fərqi tip səviyyəsində ifadə edir:
- `const T&`: hər şeyə bağlanır. "Oxuyacağam, toxunmayacağam."
- **`T&&`** (rvalue reference): yalnız rvalue-yə bağlanır. "Bu obyekt müvəqqətidir, məzmununu götürə bilərəm."

### `std::move` heç nəyi köçürmür

```cpp
std::string s = "...";
v.push_back(std::move(s));
```

`std::move(s)` sadəcə **tip çevirməsidir**: `static_cast<std::string&&>(s)`. Mənası: "bu lvalue-ni rvalue kimi qəbul et, mən ondan artıq istifadə etməyəcəyəm." Köçürməni `std::move` yox, sonra çağırılan **move constructor** edir.

### Moved-from vəziyyəti

Köçürülmüş obyekt **etibarlı, amma qeyri-müəyyən** vəziyyətdə qalır. Onu dağıtmaq və ona yeni dəyər vermək olar, amma dəyərinə güvənmək olmaz. Bizim vektor üçün bu vəziyyət dəqiqdir: **boş** (`size == 0`, `capacity == 0`) və yenidən istifadəyə yararlı. Test bunu yoxlayır: `moved-from vector is usable`.

## 3. Rule of five

A2-dəki rule of three-yə iki metod əlavə olunur:

| # | Metod | Nə vaxt | Bizdə nə edir | Mürəkkəblik |
|---|---|---|---|---|
| 1 | `~vector()` | dağıdılanda | obyektləri dağıdır, yaddaşı qaytarır | O(n) |
| 2 | `vector(const vector&)` | lvalue-dən yeni obyekt | dərin kopya | O(n) |
| 3 | `operator=(const vector&)` | lvalue-ni mövcud obyektə vermək | copy-and-swap | O(n) |
| 4 | **`vector(vector&&) noexcept`** | rvalue-dən yeni obyekt | **buffer-i oğurlayır** | **O(1)** |
| 5 | **`operator=(vector&&) noexcept`** | rvalue-ni mövcud obyektə vermək | move-and-swap | O(1) + köhnə məzmunu silmək |

**Qayda:** sinif resursu özü idarə edirsə, beşini də yazmalıdır. Move əməliyyatlarını yazmasaq, kompilyator onları **yaratmır**, çünki destructor və copy əməliyyatları özümüz yazmışıq. Onda hər "köçürmə" səssizcə kopyalamaya çevrilir.

**Rule of zero:** resursu özü idarə etməyən siniflər (məsələn, içində `mystl::vector` və `std::string` olan sinif) bu beşindən **heç birini** yazmamalıdır. Kompilyatorun yaratdığı versiyalar üzvlərin öz əməliyyatlarını çağırır və düzgün işləyir. Rule of five yalnız resursun özünü idarə edən "aşağı səviyyəli" siniflər (bizim vektor kimi) üçündür.

### Move constructor

```cpp
vector(vector&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      capacity_(std::exchange(other.capacity_, 0)) {}
```

```
əvvəl:   other.data_ ──► [ e0 | e1 | e2 ]          this: boş
sonra:   this.data_  ──► [ e0 | e1 | e2 ]          other: nullptr, 0, 0
```

- **`std::exchange(x, new)`**: `x`-ə `new` dəyərini verir və **köhnə** dəyəri qaytarır. Bir sətirdə iki iş görür: "götür və sıfırla".
- **Mənbəni sıfırlamaq vacibdir.** Sıfırlanmasa, `other`-in destructor-u eyni buffer-i siləcək. Bu, A2-dəki double free-nin özüdür.
- **Element toxunulmur:** 1 000 000 element də olsa, yalnız 3 üzv dəyişir. Test buffer ünvanının eyni qaldığını və heç bir `Tracked` obyektinin qurulmadığını yoxlayır.

### Move assignment: move-and-swap

```cpp
vector& operator=(vector&& other) noexcept {
    vector moved(std::move(other));   // 1. other-in buffer-i müvəqqəti obyektə keçir, other boşalır
    swap(moved);                      // 2. bizim köhnə buffer müvəqqəti obyektə keçir
    return *this;                     // 3. müvəqqəti obyekt köhnə məzmunu silir
}
```

Copy-and-swap ilə eyni ideyadır, yalnız kopya əvəzinə köçürmə edilir. **Self-move** (`v = std::move(v)`) də təhlükəsizdir: 1-ci addımda `v` boşalır, 2-ci addımda məzmun geri qayıdır. Test bunu yoxlayır.

**`other` niyə boş qalır?** Sadəcə `swap(other)` də olardı: bizim köhnə məzmunumuz `other`-ə keçərdi və o dağılanda silinərdi. Amma onda köhnə elementlər `other` dağılana qədər yaşayardı (bəlkə çox uzun müddət), `other` də gözlənilməz məzmun saxlayardı. `std::vector`-dan gözlənilən davranış mənbənin boşalmasıdır.

## 4. `push_back(T&&)`

```cpp
void push_back(const T& value) { append(value); }              // lvalue: kopyala
void push_back(T&& value)      { append(std::move(value)); }   // rvalue: köçür
```

Kompilyator arqumentə görə avtomatik seçir:

```cpp
v.push_back(s);              // s lvalue-dir      → const T&  → kopya
v.push_back(std::string());  // müvəqqəti obyekt  → T&&       → köçürmə
v.push_back(std::move(s));   // açıq rvalue       → T&&       → köçürmə, s boşalır
```

**`T&& value` funksiyanın içində niyə yenə `std::move(value)`?** Çünki **adı olan hər şey lvalue-dir**, tipi `T&&` olsa belə. `value`-nin adı var. Onu yenə rvalue kimi ötürmək üçün `std::move` lazımdır.

### `append` və forwarding reference

İki `push_back` eyni işi görür, fərq yalnız elementin necə qurulmasındadır. Kodu təkrarlamamaq üçün ortaq `append` var:

```cpp
template <typename U>
void append(U&& value) {
    // ...
    std::construct_at(data_ + size_, std::forward<U>(value));
}
```

Template parametrində `U&&` adi rvalue reference **deyil**, **forwarding reference**-dir: həm lvalue-yə, həm rvalue-yə bağlanır. `std::forward<U>(value)` dəyərin **əvvəlki növünü bərpa edir**: lvalue idisə kopyalanır, rvalue idisə köçürülür. Bu mövzu B4-də (`emplace_back`) ətraflı işlənəcək.

## 5. Böyümə: `std::move_if_noexcept`

Böyümə zamanı köhnə elementləri yeni buffer-ə köçürmək ən sürətli yoldur. Amma bir **təhlükə** var:

```
köhnə:  [ A | B | C | D ]
yeni:   [ A'| B'|   ...       ← C köçürülərkən exception!
```

A və B artıq köçürülüb, yəni **köhnə buffer-də boşaldılıblar**. Vektoru əvvəlki vəziyyətinə qaytarmaq mümkün deyil: onları geri köçürmək də exception ata bilər. Strong guarantee (A4) pozulur.

**Həll:** move constructor `noexcept`-dirsə (exception ata bilməz), köçür. Deyilsə, **kopyala**: kopyalama köhnə elementlərə toxunmur, exception olsa köhnə buffer olduğu kimi qalır.

```cpp
std::construct_at(new_data + built, std::move_if_noexcept(data_[built]));
```

`std::move_if_noexcept(x)`:

| `T`-nin move constructor-u | `T` kopyalana bilir? | Nə edilir | Zəmanət |
|---|---|---|---|
| `noexcept` | fərqi yoxdur | **köçürmə** | strong (köçürmə uğursuz ola bilməz) |
| `noexcept` deyil | bəli | **kopya** | strong |
| `noexcept` deyil | xeyr (move-only) | köçürmə (başqa yol yoxdur) | yalnız basic |

`std::vector` da eyni qaydaya əməl edir.

### Nümayiş: `noexcept` unudulanda (həqiqi çıxış)

İki tip arasındakı yeganə fərq move constructor-un `noexcept` olub-olmamasıdır:

```
=== move ctor IS noexcept ===
  push_back(Noisy(3))  <- full, vector grows:
      ctor  #3
      move  #3
      move  #1        ← köhnə elementlər KÖÇÜRÜLÜR
      move  #2

=== move ctor is NOT noexcept ===
  push_back(Noisy(3))  <- full, vector grows:
      ctor  #3
      move  #3
      COPY  #1        ← köhnə elementlər KOPYALANIR
      COPY  #2
```

**Nəticə:** öz tipinizin move constructor-u exception atmırsa, mütləq `noexcept` yazın. Yoxsa vektor onu böyüyərkən kopyalayacaq. Bu, səssiz performans itkisidir: kod düzgün işləyir, sadəcə yavaş. Faza D-nin 4-cü sualı bunu ölçəcək.

### Bizim vektorun `noexcept`-i niyə vacibdir?

`vector<vector<int>>` böyüyəndə daxili vektorlar `std::move_if_noexcept` ilə daşınır. Bizim move constructor-umuz `noexcept`-dir, ona görə daxili vektorlar **köçürülür**: yalnız 3 pointer/ədəd dəyişir. `noexcept` olmasaydı, hər daxili vektor bütün elementləri ilə **dərin kopyalanardı**.

Bunu yoxladıq: `noexcept`-i bilərəkdən silib testləri işlətdik:

```
TEST CASE:  move ops are noexcept
ERROR: CHECK( std::is_nothrow_move_constructible_v<mystl::vector<int>> ) is NOT correct!
TEST CASE:  vector<vector<int>> growth moves inner vectors (no deep copy)
ERROR: CHECK( &outer[0][0] == inner_buffer ) is NOT correct!
```

İkinci xəta göstərir ki, daxili vektorun buffer-i dəyişdi, yəni dərin kopyalandı. `noexcept` geri qaytarıldı.

## 6. `vector<std::unique_ptr<int>>`

`std::unique_ptr` kopyalana bilmir, yalnız köçürülə bilir. v5 ilə işləyir, çünki:
- `push_back(std::make_unique<int>(i))` `T&&` versiyasını seçir;
- böyümə `move_if_noexcept` ilə köçürür (unique_ptr-in move-u `noexcept`-dir);
- `pop_back` və destructor `unique_ptr`-i dağıdır, o da öz `int`-ini silir.

**Copy constructor niyə problem yaratmır?** Template sinifin metodu **yalnız çağırılanda** yaradılır (A3: instantiation). `vector<unique_ptr<int>>`-u kopyalamağa çalışmasaq, copy constructor heç vaxt yaradılmır və xəta da olmur. Kopyalamağa çalışsaq, kompilyasiya xətası alarıq. Bu, düzgün davranışdır.

Bu, yol xəritəsinin A5 üçün "hazırdır" şərtidir.

## 7. Testlər

| Test | Nəyi yoxlayır |
|---|---|
| move ops are noexcept | `is_nothrow_move_constructible/assignable` |
| move ctor: steals the buffer | Buffer ünvanı eynidir, mənbə boşdur |
| move ctor: O(1) | 1 000 000 element yerində qalır; `Tracked` ilə heç bir element qurulmur |
| moved-from vector is usable | Mənbə yenidən istifadə oluna bilir |
| move assignment | Hədəfin köhnə elementləri dağıdılır, buffer keçir |
| self-move | `v = std::move(v)` vektoru pozmur |
| push_back(rvalue) moves, push_back(lvalue) copies | Düzgün overload seçilir |
| push_back(std::move(v[0])) when full | A4-dəki tələ, köçürmə versiyası |
| growth moves / copies | `move_if_noexcept` hər iki halda düzgün qərar verir |
| exception in push_back / reserve | `TrackedThrowingMove` ilə: kopyalanan elementlər exception atır, vektor dəyişmir |
| `vector<std::unique_ptr<int>>` | Move-only tip: push_back, böyümə, reserve, pop_back, vektorun köçürülməsi |
| `vector<vector<int>>` growth | Daxili vektorların buffer-i yerində qalır |

**Mutation yoxlamaları:**

| Bilərəkdən salınan səhv | Tutan testlər |
|---|---|
| `std::move_if_noexcept` → `std::move` | 3 test: hər iki exception testi və "growth copies when NOT noexcept" |
| move constructor-dan `noexcept` silindi | 2 test: noexcept yoxlaması və nested vector |

**Nəticə:** 62 test, 2485 yoxlama, hamısı keçir; build-də warning yoxdur.

## 8. Faza A yekunu: v1 → v5 təkamülü

Hesabatın "Təkamül" bölməsi bu cədvəl üzərində qurula bilər:

| Versiya | Əlavə olunan | Həll etdiyi problem | Nümayiş |
|---|---|---|---|
| v1 (A1) | `int*`, `size`, `capacity`, 2× böyümə | Sabit ölçülü massiv | 1 000 000 push_back |
| v2 (A2) | copy constructor, copy assignment (copy-and-swap) | Shallow copy → double free | `0xC0000374` heap corruption |
| v3 (A3) | `template <typename T>` | Yalnız `int` | `vector<string>`, `vector<vector<int>>` |
| v4 (A4) | `::operator new` + placement new, `reserve`, `pop_back` | `new T[]`: default ctor tələbi, lazımsız obyektlər, `pop_back` dağıtmır; `push_back(v[0])` bug-ı | Noisy müqayisəsi, `Poisoned` (-1 vs 42) |
| v5 (A5) | move ctor/assignment, `push_back(T&&)`, `move_if_noexcept` | Lazımsız kopyalar, move-only tiplər | `noexcept` ilə/onsuz böyümə |

## 9. Növbəti: Faza B

Faza A-da vektorun **əsası** quruldu. Faza B-də `std::vector`-un **tam interfeysi** əlavə olunur: `at()`, iterator-lar, bütün constructor-lar, `resize`, `insert`, `erase`, `emplace_back`, müqayisə operatorları və s.

## 10. Müdafiədə gələ biləcək suallar

**Move constructor `noexcept` olmasa nə baş verir?**
Vektor böyüyəndə elementləri köçürmək əvəzinə kopyalayır (`std::move_if_noexcept`), çünki köçürmə yarıda exception atsa, strong guarantee-ni təmin etmək olmaz. Kod düzgün işləyir, amma yavaşlayır. Nümayişdə `move` yerinə `COPY` görünür, testdə isə daxili vektorların dərin kopyalandığı görünür.

**`std::move` nə edir?**
Heç nəyi köçürmür. Yalnız lvalue-ni rvalue reference-ə çevirir (`static_cast<T&&>`), beləliklə move constructor və ya move assignment seçilir. Köçürməni onlar edir.

**Vektorun move constructor-u niyə O(1)-dir?**
Elementlərə toxunmur: yalnız buffer pointer-ini, `size` və `capacity`-ni götürür, mənbəni isə sıfırlayır. Element sayından asılı deyil.

**Köçürülmüş (moved-from) obyekt hansı vəziyyətdə olur?**
Standarta görə etibarlı, amma qeyri-müəyyən vəziyyətdə: dağıtmaq və yeni dəyər vermək olar. Bizim vektor dəqiq olaraq boş qalır və yenidən istifadə oluna bilir.

**Rule of five nədir? Rule of zero nədir?**
Resursu özü idarə edən sinif beş xüsusi metodu (destructor, iki copy, iki move) özü yazmalıdır. Rule of zero isə deyir ki, qalan siniflər resursları belə siniflərə (vector, string, unique_ptr) həvalə etməli və bu metodların heç birini yazmamalıdır.

**`vector<unique_ptr<int>>` necə işləyir, axı vektorda copy constructor var?**
Template üzv funksiyası yalnız istifadə olunanda yaradılır. Vektoru kopyalamasaq, copy constructor yaranmır. Bütün digər əməliyyatlar köçürmə ilə işləyir.
