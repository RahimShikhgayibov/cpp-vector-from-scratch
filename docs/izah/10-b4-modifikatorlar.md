# B4: Modifikatorlar

**Fayllar:** [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp), [`tests/test_vector_modifiers.cpp`](../../tests/test_vector_modifiers.cpp)

## 1. Əlavə olunanlar

| Metod | Nə edir | Mürəkkəblik |
|---|---|---|
| `emplace_back(args...)` | sona elementi **birbaşa** arqumentlərdən qurur, reference qaytarır | amortized O(1) |
| `clear()` | bütün elementləri dağıdır, `capacity` qalır | O(n) |
| `emplace(pos, args...)` | `pos`-dan əvvəl birbaşa qurur | O(end − pos) |
| `insert(pos, value)` (copy və move) | `pos`-dan əvvəl bir element | O(end − pos) |
| `insert(pos, n, value)` | `n` kopya | O(n + end − pos) |
| `insert(pos, first, last)` | aralıq | O(k + end − pos) |
| `insert(pos, {...})` | initializer list | eyni |
| `erase(pos)` | bir elementi silir | O(end − pos) |
| `erase(first, last)` | aralığı silir | O(end − first) |

`push_back`, `pop_back`, `swap` artıq var idi. `push_back` indi `emplace_back`-i çağırır.

## 2. `emplace_back`: variadic template və perfect forwarding

```cpp
template <typename... Args>
reference emplace_back(Args&&... args) {
    // ...
    std::construct_at(data_ + size_, std::forward<Args>(args)...);
    // ...
    return back();
}
```

### `push_back` ilə fərq

```cpp
v.push_back(Person("Alice", 30));   // 1. müvəqqəti Person qurulur
                                    // 2. vektora KÖÇÜRÜLÜR (move ctor)
                                    // 3. müvəqqəti obyekt dağıdılır

v.emplace_back("Alice", 30);        // Person("Alice", 30) birbaşa vektorun yaddaşında
```

Test `Tracked` ilə bunu sayır: `emplace_back(7)` yalnız **1 constructor**, 0 kopya, 0 move edir. `push_back(Tracked(8))` isə 2 constructor (müvəqqəti + move) edir.

### Sintaksis

- **`typename... Args`**: **parameter pack**, yəni istənilən sayda tip. `emplace_back("Alice", 30)` üçün `Args = {const char(&)[6], int}` olur.
- **`Args&&... args`**: hər arqument **forwarding reference**-dir (A5-də `append(U&&)` kimi, indi çox arqumentlə).
- **`std::forward<Args>(args)...`**: `...` **pack expansion**-dur. Kompilyator bunu `std::forward<A1>(a1), std::forward<A2>(a2), ...` kimi açır.

### Perfect forwarding

`std::forward<A>(a)` arqumentin **əvvəlki növünü bərpa edir**:
- çağıran **lvalue** veribsə (`v.emplace_back(s)`), `T`-nin constructor-u lvalue alır, yəni **kopyalayır**;
- **rvalue** veribsə (`v.emplace_back(std::move(s))`), rvalue alır, yəni **köçürür**.

`std::forward` olmasaydı, adı olan hər şey lvalue olduğu üçün (A5) hər şey kopyalanardı. `std::move` istifadə etsəydik, çağıranın lvalue-ləri də "oğurlanardı". `std::forward` "olduğu kimi ötür" deməkdir: buna **perfect forwarding** deyilir.

**Reference qaytarmaq (C++17):** `Person& p = v.emplace_back("Alice", 30);` yeni elementlə dərhal işləməyə imkan verir.

## 3. Ortaya əlavə etmək: `emplace(pos, args...)`

```cpp
template <typename... Args>
iterator emplace(const_iterator pos, Args&&... args) {
    const size_type index = index_of(pos);
    emplace_back(std::forward<Args>(args)...);          // 1
    if (index + 1 != size_) {
        T new_element(std::move(back()));               // 2
        std::move_backward(begin() + index, end() - 1, end());   // 3
        data_[index] = std::move(new_element);          // 4
    }
    return begin() + index;
}
```

```
[ a | b | c | d ]                   emplace(begin() + 1, x)
1. sona qur:      [ a | b | c | d | x ]
2. x-i götür:     tmp = x
3. sağa sür:      [ a | b | b | c | d ]
4. yerinə qoy:    [ a | x | b | c | d ]
```

### Niyə əvvəl **sona** qururuq?

**Alias tələsi** (yol xəritəsində "klassik tələ" kimi qeyd olunub):

```cpp
v.insert(v.begin(), v.back());
```

`v.back()` reference-dir. Sadəlövh alqoritm əvvəl elementləri sağa sürüşdürsəydi, `v.back()`-in göstərdiyi yer **başqa elementlə** üzərindən yazılardı və ya köhnə buffer silinərdi. Yanlış dəyər daxil edilərdi.

Bizim alqoritmdə **1-ci addım** yeni elementi `emplace_back` ilə qurur. Bu anda heç nə sürüşdürülməyib, `v.back()` hələ düzgün yerdədir. Böyümə lazımdırsa, `reallocate_with_tail` (B3) yeni elementi köhnə buffer dağıdılmamışdan əvvəl qurur. Sonrakı addımlar **artıq qurulmuş kopya** ilə işləyir.

Testlər bunu **hər iki halda** yoxlayır: yenidən ayırma ilə (`size == capacity`) və onsuz (`reserve(10)`).

**Mürəkkəblik:** `end − pos` köçürmə (+ 2). Sona əlavə amortized O(1)-dir, əvvələ əlavə isə O(n). Vektorun əvvəlinə tez-tez əlavə etmək lazımdırsa, başqa konteyner (`std::deque`) daha uyğundur. Faza D-nin 5-ci sualı bunu ölçəcək.

### `iterator` yox, `index`

```cpp
const size_type index = index_of(pos);   // pos - cbegin()
```

`pos` iterator-dur, yəni ünvandır. 1-ci addımda böyümə olsa, köhnə buffer silinir və `pos` **etibarsız** olur. İndeks isə buffer-dən asılı deyil. Ona görə əvvəlcə indeksə çeviririk, sonda `begin() + index` qaytarırıq.

## 4. Çoxlu element: "sona qur, sonra rotate"

```cpp
iterator insert(const_iterator pos, size_type count, const T& value) {
    const size_type index = index_of(pos);
    const size_type old_size = size_;
    append_n(count, [&value](T* p) { std::construct_at(p, value); });
    std::rotate(begin() + index, begin() + old_size, end());
    return begin() + index;
}
```

```
[ a | b | c ]                    insert(begin() + 1, 2, x)
append_n:   [ a | b | c | x | x ]
rotate:     [ a | x | x | b | c ]
             └─┘ └────────────┘
                 [b c][x x] → [x x][b c]
```

`std::rotate(first, middle, last)` `[first, last)` aralığını elə fırladır ki, `middle` başa keçir. O(n)-dir.

**Üstünlükləri:**
- **Alias təhlükəsizdir:** `v.insert(v.begin(), 3, v.back())`. Kopyalar heç nə sürüşdürülməmişdən əvvəl qurulur.
- **Strong guarantee əlavə mərhələsində:** `append_n` uğursuz olsa, vektor dəyişmir.
- **Bir alqoritm hər üç `insert` üçün:** `n` kopya, forward aralıq, input aralıq.
- **Input iterator:** sayı bilmirik, bir-bir `emplace_back` edirik. Böyümə olsa belə, indeks etibarlı qalır. Exception olsa, əlavə olunanlar geri alınır.

**Trade-off:** `std::rotate` hər elementi bir neçə dəfə köçürür. `libstdc++` daha mürəkkəb, birbaşa sürüşdürən alqoritm istifadə edir və bir az daha sürətlidir. Biz sadəlik və alias təhlükəsizliyini seçmişik. Benchmark bu fərqi göstərəcək.

**Şərt:** `insert(pos, first, last)`-da `first` və `last` **bu vektorun** iterator-ları olmamalıdır. `std::vector`-da da belədir: əlavə zamanı buffer dəyişə bilər.

### `append_n`: ortaq "sona n element"

B3-dəki `resize_impl`-in böyümə hissəsi ayrıca funksiyaya çıxarıldı. İndi `resize`, `insert(pos, n, value)` və forward aralıq `insert`-i onu istifadə edir:

```cpp
template <typename ConstructOne>
void append_n(size_type count, ConstructOne construct_one);
```

- **`count > max_size() - size_`** yoxlaması: `size_ + count` `size_t`-ni **daşdırmasın** deyə çıxma ilə yoxlanılır. `size_ + count > max_size()` yazsaq, toplamanın özü daşıb kiçik ədəd verə bilərdi.
- Yer yoxdursa, `reallocate_with_tail`; varsa, yerində qurulur, exception olsa geri alınır.

## 5. `erase`

```cpp
iterator erase(const_iterator pos) {
    const size_type index = index_of(pos);
    std::move(begin() + index + 1, end(), begin() + index);   // sola sür
    pop_back();                                               // sonuncu artıq lazım deyil
    return begin() + index;
}
```

```
[ a | b | c | d ]          erase(begin() + 1)
std::move:  [ a | c | d | d ]     (sonuncu "d" köçürülmüş, boş qabıqdır)
pop_back:   [ a | c | d ]
```

Silinən element **dağıdılmır**: onun üzərinə növbəti element **köçürülür** (move assignment). Sonda artıq qalan son obyekt dağıdılır. Nəticədə dağıdılan obyektlərin sayı silinən elementlərin sayına bərabər olur. Test `Tracked::live()` ilə yoxlayır.

`erase(first, last)` eyni ideyadır: `count` addım sola sürüşdürülür, sonra son `count` obyekt dağıdılır.

**Qaytarılan iterator** silinən elementdən **sonrakı** elementi göstərir. Ona görə dövrdə silmək belə yazılır:

```cpp
for (auto it = v.begin(); it != v.end(); ) {
    if (should_remove(*it)) it = v.erase(it);   // ++it YOX
    else ++it;
}
```

### Erase–remove idiomu

```cpp
v.erase(std::remove(v.begin(), v.end(), 2), v.end());
```

`std::remove` əslində heç nəyi silmir: saxlanılacaq elementləri əvvələ yığır və "yeni sonu" qaytarır. `erase` isə həmin sondan sonrakıları silir. Nəticə: O(n), dövrdə bir-bir `erase` edəndə isə O(n²) olardı. Test bunu yoxlayır. B5-də bunun C++20 forması (`mystl::erase`) əlavə olunacaq.

## 6. `clear()`

```cpp
void clear() noexcept {
    std::destroy_n(data_, size_);
    size_ = 0;
}
```

Yaddaş qaytarılmır, çünki vektor çox vaxt yenidən doldurulur. Yaddaşı qaytarmaq üçün `clear()` + `shrink_to_fit()` istifadə olunur.

## 7. Exception safety

| Əməliyyat | Zəmanət |
|---|---|
| `emplace_back`, `push_back` | **strong** |
| `insert`/`emplace` **sona** | **strong** |
| `insert`/`emplace` **ortaya** | əlavə mərhələsi strong; sürüşdürmə zamanı `T`-nin move-u exception atsa, **basic**: sızma yoxdur, vektor etibarlıdır, amma sıra qarışıq ola bilər |
| `erase`, `clear` | `T`-nin move assignment-i exception atmırsa, exception yoxdur |

`std::vector` da ortaya əlavədə yalnız basic guarantee verir: ortaya əlavə etmək üçün mövcud elementləri **yerində** sürüşdürmək lazımdır. Sürüşdürmə yarıda dayansa, onu geri qaytarmaq üçün yenə elementləri köçürmək lazım gəlir, o da exception ata bilər.

Test: ortaya `insert` zamanı exception sızma yaratmır.

## 8. Testlər

| Qrup | Nəyi yoxlayır |
|---|---|
| `emplace_back` | arqumentlərdən qurur, reference qaytarır; 0 kopya, 0 move; `emplace_back(v[0])` dolu vektorda |
| `clear` | hamısı dağıdılır, `capacity` qalır, istifadəyə yararlı qalır |
| tək `insert` | əvvəl, orta, son, boş vektor; yenidən ayırma ilə/onsuz eyni nəticə; rvalue köçürülür |
| alias tələsi | `insert(begin, back())` və `insert(begin, 3, back())`, hər biri yenidən ayırma ilə və onsuz |
| `emplace(pos, ...)` | ortaya birbaşa qurmaq |
| çoxlu `insert` | `n` kopya, `n = 0`, `std::list`, `istream_iterator`, boş aralıq, initializer list |
| `erase` | əvvəl/orta/son, qaytarılan iterator, aralıq, boş aralıq, hamısı; dağıdılan obyektlərin sayı; erase–remove |
| exception | ortaya `insert` zamanı sızma yoxdur |
| **differential test** | 5000 təsadüfi əməliyyat (`push_back`, `pop_back`, `insert`, `erase`, `resize`, `clear`), hər addımdan sonra `std::vector` ilə müqayisə |

**Mutation yoxlaması:** `reallocate_with_tail`-də sıra bilərəkdən dəyişdirildi: əvvəl köhnələr köçürüldü, sonra yeni element quruldu. **9 test** xəta verdi. Bunlar `push_back(v[0])`, `emplace_back(v[0])`, `insert(begin, back())`, `insert(begin, 3, back())`, `resize(5, v[0])` və onların exception testləridir. Alias qorunması bütün yollarla yoxlanılır. Sıra geri qaytarıldı.

**Nəticə:** 135 test, 7746 yoxlama, hamısı keçir; build-də warning yoxdur.

## 9. Müdafiədə gələ biləcək suallar

**`push_back` ilə `emplace_back`-in fərqi?**
`push_back` hazır obyekt alır və onu kopyalayır və ya köçürür. `emplace_back` constructor arqumentlərini alır və obyekti birbaşa vektorun yaddaşında qurur: müvəqqəti obyekt yaranmır.

**Perfect forwarding nədir?**
Arqumentləri növünü (lvalue/rvalue) dəyişmədən başqa funksiyaya ötürmək: `std::forward<Args>(args)...`. lvalue kopyalanır, rvalue köçürülür.

**`v.insert(v.begin(), v.back())` niyə təhlükəlidir və necə həll etmisiniz?**
`v.back()` vektorun öz elementinə reference-dir. Elementlər sürüşdürüləndə və ya buffer dəyişəndə həmin reference başqa dəyəri və ya silinmiş yaddaşı göstərir. Biz yeni elementi əvvəlcə sona qururuq (o vaxt heç nə dəyişməyib), sonra yerinə daşıyırıq.

**Ortaya `insert` niyə O(n)-dir?**
Mövqedən sonrakı bütün elementlər bir addım sağa sürüşdürülməlidir. Elementlər ardıcıl yaddaşda olduğu üçün "araya yer açmaq" başqa yolla mümkün deyil.

**`erase` iterator-ları necə təsir edir?**
Silinən mövqedən sonrakı bütün iterator-lar və reference-lər etibarsız olur (elementlər sürüşür). Ondan əvvəlkilər etibarlı qalır. Ona görə `erase` növbəti etibarlı iterator-u qaytarır.

**Erase–remove idiomu nədir?**
`std::remove` saxlanılacaq elementləri əvvələ yığır və yeni sonu qaytarır, `erase` isə qalanları silir. Bir keçiddə, O(n). Dövrdə bir-bir silmək O(n²) olardı.
