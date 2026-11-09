# A3: v3 — Template: `mystl::vector<T>`

**Fayllar:**
- [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp): `int_vector.hpp`-dən çevrildi (`git mv` ilə, tarixi qorunur)
- [`tests/test_vector.cpp`](../../tests/test_vector.cpp), [`tests/test_vector_copy.cpp`](../../tests/test_vector_copy.cpp)

## 1. Problem

`IntVector` yalnız `int` saxlayır. `double`, `std::string` və ya `Point` üçün eyni kodu kopyalayıb `int`-i dəyişmək lazım gələrdi. Bu, üç problem yaradır: kod təkrarlanır, bir yerdə düzəldilən bug digərlərində qalır, yeni tip üçün hər dəfə yeni sinif yazmaq lazım olur.

**Template** kompilyatora deyir: "bu kodu `T` tipi üçün yaz, `T`-nin nə olduğunu istifadə yerində öyrənəcəksən."

## 2. Çevrilmə: nə dəyişdi?

| `IntVector` (v2) | `vector<T>` (v3) |
|---|---|
| `class IntVector` | `template <typename T> class vector` |
| `int* data_` | `T* data_` |
| `void push_back(int value)` | `void push_back(const T& value)` |
| `int& operator[](...)` | `reference operator[](...)`, yəni `T&` |
| `new int[n]` | `new T[n]` |
| `std::size_t` | `size_type` (alias) |

Məntiq **heç dəyişmədi**: invariantlar, 2× böyümə, deep copy, copy-and-swap eynidir. Yalnız tip parametrləşdirildi.

## 3. Sintaksis

```cpp
template <typename T>
class vector {
    T* data_ = nullptr;
    // ...
};

mystl::vector<int> a;            // T = int
mystl::vector<std::string> b;    // T = std::string
```

- **`typename` və ya `class`?** `template <class T>` də eyni şeydir. `typename` daha dəqiqdir, çünki `T` sinif olmaya da bilər (`int`).
- **Sinifin içində `vector` yazanda** kompilyator onu avtomatik `vector<T>` kimi başa düşür. Buna **injected class name** deyilir. Ona görə copy constructor-u `vector(const vector& other)` yazırıq, `vector<T>(const vector<T>&)` yox.

## 4. Template necə işləyir: instantiation

Template **kod deyil, kod üçün qəlibdir**. Kompilyator `mystl::vector<int>` görəndə qəlibdə `T`-ni `int` ilə əvəz edib **ayrıca sinif** yaradır. `vector<std::string>` görəndə ikinci bir sinif yaradır. Bu prosesə **instantiation** deyilir.

```
template <typename T> class vector { ... }      ← qəlib (kompilyasiya olunmur)
        │
        ├── vector<int>          ← ayrıca sinif, ayrıca maşın kodu
        ├── vector<std::string>  ← ayrıca sinif
        └── vector<vector<int>>  ← ayrıca sinif (daxilindəki vector<int>-dən istifadə edir)
```

Nəticələri:
- **Sürət:** hər tip üçün xüsusi kod yaranır, işləmə vaxtında tip yoxlaması yoxdur. `vector<int>` əl ilə yazılmış `IntVector` qədər sürətlidir.
- **Kod həcmi (code bloat):** çoxlu fərqli `T` icra olunan faylı böyüdür.
- **Yalnız istifadə olunan metodlar yaradılır.** Heç vaxt çağırılmayan metod üçün kod yaranmır.

## 5. Template-lər niyə header-də yaşayır?

C++-da hər `.cpp` faylı **ayrıca** kompilyasiya olunur (translation unit), sonra linker onları birləşdirir. Template-i `.cpp`-yə qoysaq nə olacağını sınadıq:

```cpp
// twice.hpp
template <typename T> T twice(T x);                    // yalnız elan

// twice.cpp
template <typename T> T twice(T x) { return x + x; }   // tərif

// main.cpp
#include "twice.hpp"
int main() { return twice(21); }
```

Bu kompüterdəki həqiqi nəticə:

```
main.cpp:(.text+0x13): undefined reference to `int twice<int>(int)'
```

Səbəb belədir:
- `main.cpp` kompilyasiya olunanda `twice<int>` lazımdır, amma kompilyator yalnız elanı görür, tərifi yox. Ona görə "başqa faylda olar" deyə gözləyir.
- `twice.cpp` kompilyasiya olunanda tərif görünür, amma bu faylda heç kim `twice<int>` istəmir. Ona görə heç bir instantiation yaranmır.
- Linker `twice<int>`-i heç yerdə tapmır.

**Həll:** template-in tam tərifi header-də olur, hər istifadəçi `.cpp` onu görür və lazımi instantiation-u özü yaradır. Kitabxanamızın **header-only** olmasının (`add_library(mystl INTERFACE)`) səbəbi budur.

Eyni instantiation bir neçə `.cpp`-də yaransa, linker dublikatları birləşdirir. Bu, template-lər üçün xüsusi qaydadır.

## 6. Tip adları (member type aliases)

```cpp
using value_type      = T;
using size_type       = std::size_t;
using reference       = T&;
using const_reference = const T&;
```

`std::vector`-da da bu adlar var. Faydası: ümumi kod konteynerin içinə baxmadan tipləri öyrənə bilir. Məsələn, `typename V::value_type` istənilən konteynerin element tipini verir, `std::vector` olsun, `mystl::vector` olsun. Faza B-də iterator-lar üçün daha çox alias əlavə olunacaq. Test `std::is_same_v` ilə alias-ların düzgün olduğunu yoxlayır.

## 7. `push_back(const T&)`, niyə `push_back(T)` yox?

`int` üçün fərq yoxdur. Amma `T = std::string` olsa, dəyərlə ötürmək **arqumentin kopyasını** yaradar, sonra o kopya yenidən massivə kopyalanar: iki kopya. `const T&` ilə arqument kopyalanmadan ötürülür və yalnız massivə yazılarkən bir dəfə kopyalanır.

A5-də `push_back(T&&)` əlavə olunacaq: müvəqqəti obyektlər üçün sıfır kopya.

## 8. `T`-dən nə tələb olunur? (gizli tələblər)

Template kodu `T` ilə müəyyən əməliyyatlar edir. `T` onları dəstəkləmirsə, kompilyasiya xətası çıxır. v3-də tələblər bunlardır:

| Əməliyyat | Harada | Tələb |
|---|---|---|
| `new T[n]` | `grow`, copy constructor | **default constructor** |
| `data_[i] = x` | `push_back`, `grow`, copy | **copy assignment** |
| `delete[] data_` | destructor | destructor |

**Default constructor-u olmayan tip** üçün həqiqi xəta:

```cpp
struct NoDefault { explicit NoDefault(int v) : value(v) {} int value; };
mystl::vector<NoDefault> v;
v.push_back(NoDefault{1});
```
```
vector.hpp:87: error: no matching function for call to 'NoDefault::NoDefault()'
```

87-ci sətir `T* new_data = new T[new_capacity];`-dir. `std::vector<NoDefault>` isə problemsiz işləyir. Bu, **A4-ün mövzusudur**: yaddaşı obyektlərdən ayırmaq.

**`vector<vector<int>>` niyə işləyir?** Daxili `vector<int>` bütün tələbləri ödəyir: default constructor-u, copy assignment-i (A2!) və destructor-u var. **A2 olmasaydı**, kompilyatorun shallow copy-si ilə `vector<vector<int>>` hər grow-da double free edərdi. Rule of three-nin dəyəri burada görünür: bizim tipimiz özü də başqa konteynerin elementi ola bilir.

## 9. Testlər

Testlər yol xəritəsinin "hazırdır" şərtini yoxlayır: `vector<int>`, `vector<std::string>` və `vector<vector<int>>` testləri keçməlidir.

| Qrup | Nəyi tutur |
|---|---|
| `vector<int>` | v1/v2-nin bütün testləri, template-ə çevrilmə heç nəyi pozmayıb |
| tip alias-ları | `value_type`, `size_type`, `reference`, `const_reference` düzgündür |
| `vector<std::string>` | Heap-ə sahib olan element. 1000 element ilə çoxlu grow-dan sonra sətirlər pozulmur |
| `push_back does not alias` | Vektor arqumentin **kopyasını** saxlayır, onun özünü yox |
| `vector<vector<int>>` | Fərqli uzunluqlu sətirlərdən ibarət 2D struktur; daxili vektorları yerində dəyişmək |
| deep copy (nested) | Xarici vektorun kopyası daxili vektorları da dərin kopyalayır |
| `double`, `struct Point` | İstifadəçinin öz tipi də işləyir |

**Testlərdə niyə uzun sətirlər?** `std::string` qısa sətirləri (GCC-də 15 simvola qədər) öz içində saxlayır, heap istifadə etmir. Buna **Small String Optimization (SSO)** deyilir. Kopyalama səhvləri (shallow copy, use-after-free) yalnız heap-dəki sətirlərdə özünü göstərir. Ona görə `long_string()` SSO-ya sığmayan sətirlər yaradır.

**Nəticə:** 28 test, 2141 yoxlama, hamısı keçir; build-də warning yoxdur.

## 10. Bilinən məhdudiyyətlər

1. **Default constructor tələbi və lazımsız obyektlər.** `new T[n]` `capacity` qədər obyekt qurur, `size` qədər yox. `vector<std::string>` 1000 elementdə 1024 capacity ilə 24 lazımsız boş sətir saxlayır. → **A4**
2. **Exception safety.** `T`-nin kopyalanması exception ata bilər (məsələn, `std::string` yaddaş tapmasa `std::bad_alloc` atır).
   - `grow()`-da kopyalama zamanı exception olsa, `new_data` heç vaxt silinmir: **sızma**.
   - Copy constructor-da exception olsa, obyekt tam qurulmayıb, ona görə destructor çağırılmır, ayrılmış massiv **sızır**.
   - `int` üçün bu problem yoxdur (kopyalama exception atmır), ümumi `T` üçün isə var. → **A4** və **B6** (strong exception guarantee)
3. **Grow kopyalayır.** `vector<vector<int>>` böyüyəndə **hər daxili vektor dərin kopyalanır**, yəni bütün daxili elementlər. Halbuki köhnə daxili vektorlar onsuz da silinəcək: onların massivini sadəcə "götürmək" olardı. → **A5** (move semantics)

## 11. Müdafiədə gələ biləcək suallar

**Template nədir, niyə lazımdır?** Tipdən asılı olmayan kod qəlibidir. Bir dəfə yazılır, kompilyator hər istifadə olunan tip üçün ayrıca versiya yaradır. Kod təkrarı olmur, sürət itkisi də yoxdur.

**Template-lər niyə header-də olur?** Hər `.cpp` ayrıca kompilyasiya olunur. Instantiation istifadə yerində baş verir, ona görə kompilyator orada tam tərifi görməlidir. Tərif `.cpp`-də olsa, linker "undefined reference" xətası verir. Bunu sınaqla göstərmişik.

**Instantiation nədir?** Kompilyatorun template-də `T`-ni konkret tiplə əvəz edib real sinif və ya funksiya yaratmasıdır.

**`vector<T>` hansı `T`-lərlə işləyir?** v3-də `T` default constructor, copy assignment və destructor-a malik olmalıdır. Default constructor tələbi `new T[]`-dən gəlir və A4-də aradan qaldırılır.

**`vector<vector<int>>` niyə düzgün işləyir?** Daxili vektor rule of three-ni ödəyir. Kopyalananda dərin kopyalanır, ona görə xarici vektorun grow-u və kopyası double free etmir.

**`push_back` niyə `const T&` qəbul edir?** Böyük tiplərin lazımsız kopyalanmasının qarşısını almaq üçün. Arqument kopyalanmadan ötürülür, yalnız vektora yazılarkən bir dəfə kopyalanır.
