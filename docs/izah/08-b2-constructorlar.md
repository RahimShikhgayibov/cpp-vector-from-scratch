# B2: Constructor-lar və mənimsəmə

**Fayllar:** [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp), [`tests/test_vector_construct.cpp`](../../tests/test_vector_construct.cpp)

## 1. Əlavə olunanlar

| İmza | Nə edir | Mürəkkəblik |
|---|---|---|
| `explicit vector(n)` | `n` ədəd value-initialized element (`int` → 0, `string` → "") | O(n) |
| `vector(n, value)` | `n` ədəd `value` kopyası | O(n) |
| `vector(first, last)` | iterator aralığının kopyası | O(n) |
| `vector({a, b, c})` | initializer list | O(n) |
| `operator=({a, b, c})` | initializer list ilə mənimsəmə | O(n) |
| `assign(n, value)`, `assign(first, last)`, `assign({...})` | məzmunu tam əvəz edir | O(n) |
| deduction guide | `mystl::vector v(it1, it2)` → `T`-ni iterator-dan çıxarır | — |

Default, copy və move constructor-lar Faza A-dan gəlir.

## 2. Ən vacib texnika: constructor delegation və exception safety

```cpp
vector(size_type count, const T& value) : vector() {   // ← delegation
    reserve(count);
    for (; size_ < count; ++size_) {
        std::construct_at(data_ + size_, value);
    }
}
```

**Problem:** constructor-un gövdəsində exception olsa (məsələn, 4-cü kopya uğursuz olsa), C++ qaydasına görə **obyekt heç vaxt yaranmamış sayılır** və onun destructor-u **çağırılmır**. Artıq qurulmuş 3 element və ayrılmış yaddaş isə sızır.

**Həll:** `: vector()` əvvəlcə default constructor-u çağırır. C++ qaydasına görə **delegate edilən constructor bitən kimi obyekt "qurulmuş" sayılır**. Sonra gövdədə exception olsa, destructor **çağırılır**: `size_` qədər elementi dağıdır və yaddaşı qaytarır.

`size_`-in dövrdə **bir-bir** artırılması da vacibdir. Destructor dəqiq bilir ki, neçə element qurulub. `construct_at` exception atsa, `++size_` işləmir, ona görə yarımçıq element sayılmır.

**Yoxlama (mutation):** `: vector()` silinəndə testlər sızmanı dərhal tutdu:

```
TEST CASE:  vector(count, value): exception while copying leaks nothing
tests/test_vector_construct.cpp:98: ERROR: CHECK( Tracked::live() == 1 ) is NOT correct!
TEST CASE:  assign: exception leaves the vector unchanged
tests/test_vector_construct.cpp:259: ERROR: CHECK( Tracked::live() == 0 ) is NOT correct!
```

Delegation geri qaytarıldı. `assign` də sızdı, çünki o, daxildə `vector(n, value)` istifadə edir.

## 3. `explicit vector(n)`

```cpp
explicit vector(size_type count);
```

`explicit` olmasaydı, bu yazılış kompilyasiya olunardı:

```cpp
mystl::vector<int> v = 5;          // 5 elementli vektor?! çaşdırıcıdır
void f(const mystl::vector<int>&);
f(5);                              // səssizcə 5 elementli vektor yaranardı
```

`explicit` belə gizli çevirmələri qadağan edir. Test: `std::is_convertible_v<std::size_t, mystl::vector<int>>` `false`-dur.

**Value-initialization:** `std::construct_at(p)` (arqumentsiz) `T()` çağırır. `int` üçün bu, **0** deməkdir, "zibil" dəyər yox. Test bütün elementlərin 0 olduğunu yoxlayır.

## 4. Klassik tələ: `(5, 1)` və `{5, 1}`

```cpp
mystl::vector<int> a(5, 1);   // 5 ədəd 1:      [1, 1, 1, 1, 1]
mystl::vector<int> b{5, 1};   // iki element:    [5, 1]
```

Fiqurlu mötərizə ilə initializer_list constructor-u **həmişə üstünlük alır**, əgər uyğun gəlirsə. `std::vector` da eyni davranır. Bu, müsahibələrdə tez-tez soruşulan sualdır.

## 5. Iterator aralığı constructor-u və konseptlər

```cpp
template <std::input_iterator InputIt>
vector(InputIt first, InputIt last);
```

### Niyə `std::input_iterator` konsepti?

Konsept olmasaydı (sadəcə `template <typename InputIt>`):

```cpp
mystl::vector<int> v(5, 1);
```

Kompilyator iki namizədə baxardı:
- `vector(size_type, const int&)`: `5` `int`-dən `size_t`-yə çevrilməlidir;
- `vector(InputIt, InputIt)` `InputIt = int` ilə: **dəqiq uyğunluq**, çevirmə lazım deyil.

İkincisi seçilərdi və `int`-ləri iterator kimi istifadə etməyə çalışardı (`*first`): kompilyasiya xətası. C++20 **konsepti** template-i yalnız həqiqi iterator-lar üçün aktiv edir. `int` iterator deyil, ona görə birinci constructor seçilir. C++20-dən əvvəl bu, `std::enable_if` ilə edilirdi: daha çətin oxunan texnika.

### Forward və input iterator fərqi

```cpp
if constexpr (std::forward_iterator<InputIt>) {
    reserve(std::distance(first, last));   // əvvəl say
    // ... bir dəfə ayrılmış yerdə qur
} else {
    for (; first != last; ++first) append(*first);   // bir-bir əlavə et
}
```

| Iterator növü | Nümunə | Neçə dəfə keçmək olar | Strategiya |
|---|---|---|---|
| **forward** (və güclüləri) | `T*`, `std::list`, `std::forward_list` | çox dəfə | əvvəl say (`distance`), bir dəfə `reserve`, sonra qur: **bir ayırma** |
| **input** | `std::istream_iterator` (fayl və ya klaviatura) | **bir dəfə** | sayı bilmək mümkün deyil: bir-bir `push_back`, amortized böyümə |

Input iterator-u saymaq üçün keçsək, məlumat **itər**: axından oxunan rəqəmlər geri qayıtmır. Test `istringstream`-dən oxuyur.

`if constexpr` hansı budağın lazım olduğunu **kompilyasiya vaxtı** seçir. Lazım olmayan budaq bu tip üçün ümumiyyətlə kompilyasiya olunmur.

Test: `std::forward_list`-dən 4 element alınanda `capacity == 4` olur, yəni dəqiq bir ayırma edilib.

### Tip çevirməsi

`*first`-in tipi `T` olmaya da bilər:

```cpp
const char* words[] = {"one", "two"};
mystl::vector<std::string> v(std::begin(words), std::end(words));   // const char* → string
```

`std::construct_at(p, *first)` `T`-nin uyğun constructor-unu çağırır. `std::vector` da belə edir.

## 6. initializer_list

```cpp
vector(std::initializer_list<T> init) : vector(init.begin(), init.end()) {}
```

`std::initializer_list<T>` `{1, 2, 3}` yazılışından kompilyatorun yaratdığı kiçik, **yalnız oxuna bilən** massivdir. Onun `begin()`/`end()`-i pointer-dir, ona görə iterator constructor-una **delegate** edirik. Kod təkrarı olmur.

**Bir məhdudiyyət:** initializer_list-in elementləri `const`-dur, ona görə həmişə **kopyalanır**, köçürülə bilməz. `mystl::vector<std::unique_ptr<int>> v{std::make_unique<int>(1)};` kompilyasiya olunmur. `std::vector`-da da eynidir.

İç-içə istifadə də işləyir: `mystl::vector<mystl::vector<int>> grid{{1, 2}, {3}, {}};`

## 7. CTAD (class template argument deduction)

```cpp
mystl::vector v{1, 2, 3};                    // vector<int>: constructor-dan avtomatik
mystl::vector w(list.begin(), list.end());   // vector<double>: deduction guide lazımdır
```

Iterator constructor-unda `T` parametrlərdə görünmür (`InputIt` var). Ona görə kompilyatora necə çıxaracağını göstəririk:

```cpp
template <std::input_iterator InputIt>
vector(InputIt, InputIt) -> vector<std::iter_value_t<InputIt>>;
```

`std::iter_value_t<It>` iterator-un göstərdiyi tipdir.

## 8. `assign` və `operator=(initializer_list)`

```cpp
void assign(size_type count, const T& value) {
    vector replacement(count, value);
    swap(replacement);
}
```

**"Qur, sonra swap"**, copy-and-swap ilə eyni ideyadır. Üstünlükləri:
- **Strong guarantee:** exception olsa, `*this` toxunulmaz qalır. Test bunu yoxlayır.
- **Alias təhlükəsizliyi:** `v.assign(4, v[0])`. `v[0]` yeni vektor tam qurulana qədər yaşayır.
- Kod sadədir və təkrarlanmır.

**Trade-off:** `std::vector::assign` köhnə buffer yetərlidirsə, onu **təkrar istifadə edir** (yeni yaddaş ayırmır). Bizim versiya həmişə yeni buffer ayırır. Bu, sadəlik və strong guarantee üçün ödənilən qiymətdir, hesabatın "trade-off-lar" bölməsində qeyd olunmalıdır.

## 9. Testlər

| Qrup | Nəyi yoxlayır |
|---|---|
| `vector(n)` | `int` sıfırlanır, `string` boşdur, default ctor dəqiq `n` dəfə çağırılır, `vector(0)` heç nə ayırmır, `explicit` |
| `vector(n, value)` | kopyalar; `(5, 1)` və `{5, 1}` fərqi; exception-da sızma yoxdur |
| `vector(first, last)` | C massivi, `std::array`, `std::list`, `std::forward_list`, `istream_iterator` (input), `const char*` → `string`, boş aralıq, exception-da sızma yoxdur |
| initializer_list | sadə, boş `{}`, iç-içə |
| CTAD | `vector{1,2,3}` → `vector<int>`, list iterator-ları → `vector<double>` |
| `operator=`, `assign` | hər üç forma; `assign(4, v[0])` aliası; exception-da vektor dəyişmir |

**Nəticə:** 100 test, 2615 yoxlama, hamısı keçir; build-də warning yoxdur.

## 10. Müdafiədə gələ biləcək suallar

**Constructor-da exception olsa, destructor çağırılırmı?**
Xeyr, obyekt tam qurulmayıbsa, çağırılmır. Ona görə constructor-lar əvvəlcə default constructor-a delegate edir: delegation bitəndən sonra obyekt qurulmuş sayılır və exception olsa, destructor artıq qurulmuş elementləri təmizləyir. Bunu mutation testi ilə yoxlamışıq.

**`vector<int>(5, 1)` və `vector<int>{5, 1}`-in fərqi?**
Birincisi 5 ədəd 1, ikincisi iki elementli `[5, 1]` vektorudur. Fiqurlu mötərizədə initializer_list constructor-u üstünlük alır.

**Iterator constructor-unda konsept niyə lazımdır?**
Konseptsiz `vector<int>(5, 1)` çağırışında template versiya (`InputIt = int`) dəqiq uyğunluq kimi seçilərdi. `std::input_iterator` onu yalnız həqiqi iterator-lar üçün aktiv edir.

**Input iterator ilə forward iterator-un fərqi nədir və nə üçün vacibdir?**
Input iterator yalnız bir dəfə keçilə bilər (məsələn, axından oxumaq), ona görə elementləri əvvəlcədən saymaq olmaz və bir-bir əlavə etmək lazımdır. Forward iterator-u isə əvvəlcə sayıb bir dəfə yaddaş ayırmaq olur.

**`vector(n)` niyə `explicit`-dir?**
`vector<int> v = 5;` və ya `f(5)` kimi gözlənilməz, gizli çevirmələrin qarşısını almaq üçün.
