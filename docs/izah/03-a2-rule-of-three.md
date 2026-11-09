# A2: v2 — Kopyalama və rule of three

**Fayllar:**
- [`experiments/a2_shallow_copy.cpp`](../../experiments/a2_shallow_copy.cpp): problemin nümayişi
- [`include/mystl/int_vector.hpp`](https://github.com/RahimShikhgayibov/cpp-vector-from-scratch/blob/b8d27b531c54266742230fe602de57e5f61ed1dc/include/mystl/int_vector.hpp): həlli (A2 commit-indəki vəziyyət)
- [`tests/test_int_vector_copy.cpp`](https://github.com/RahimShikhgayibov/cpp-vector-from-scratch/blob/b8d27b531c54266742230fe602de57e5f61ed1dc/tests/test_int_vector_copy.cpp): testlər

> A3-də bu fayllar `vector.hpp` və `test_vector_copy.cpp` adı ilə template-ə çevrildi. Linklər v2-nin olduğu commit-ə aparır.

## 1. Problem: kompilyatorun yaratdığı copy constructor

v1-də copy constructor yazmamışdıq. Belə olanda kompilyator onu **özü yaradır**. Yaradılan versiya hər üzvü bir-bir kopyalayır (**memberwise copy**):

```cpp
// kompilyatorun yazdığı, təxminən:
IntVector(const IntVector& other)
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {}
```

`size_` və `capacity_` üçün bu, düzgündür. Amma `data_` pointer-dir. Pointer-i kopyalamaq **ünvanı** kopyalamaq deməkdir, massivin özü kopyalanmır:

```
IntVector b = a;   (shallow copy)

a.data_ ──┐
          ├──► [ 1 | 2 | 3 | ? ]     bir massiv, iki sahib
b.data_ ──┘
```

### Nümayişin həqiqi çıxışı (Windows)

```
1) Addresses of the arrays:
    a.data() = 0x2559d6f60c0
    b.data() = 0x2559d6f60c0   <- SAME address!

2) b[0] = 100, then read a[0]:
    a[0] = 100   <- a changed too!

3) End of block: b is destroyed
    ~ShallowIntVector: delete[] 0x2559d6f60c0

4) End of main: a is destroyed -> SECOND delete[] of the same address
    ~ShallowIntVector: delete[] 0x2559d6f60c0
exit code: 0xC0000374
```

Burada üç ayrı səhv görünür:

1. **Asılılıq.** `b`-ni dəyişmək `a`-nı da dəyişir. Kopya müstəqil deyil.
2. **Dangling pointer.** `b` dağıdılanda massivi sildi. `a` indi **silinmiş yaddaşı** göstərir. Ondan oxumaq use-after-free olardı.
3. **Double free.** `a` dağıdılanda eyni ünvanı **ikinci dəfə** silir. Bu, undefined behavior-dur.
   - Windows-da proqram `0xC0000374` (`STATUS_HEAP_CORRUPTION`) kodu ilə çökdü. Windows heap meneceri pozulmuş heap-i aşkarladı.
   - Linux-da ASan dəqiq mesaj verir: `attempting double-free`, hansı sətirdə ayrıldığı, harada birinci və ikinci dəfə silindiyi.
   - Undefined behavior-un nəticəsi zəmanətli deyil: proqram çökə də bilər, səssizcə davam edə də bilər. Səssiz hal daha təhlükəlidir.

**`std::unitbuf` niyə?** `std::cout` mətni əvvəlcə buferə yığır, sonra ekrana çıxarır. Proqram çökəndə buferdə qalan mətn itir. İlk sınaqda heç bir çıxış görmədik məhz buna görə. `unitbuf` hər yazını dərhal ekrana çıxarır.

## 2. Həll: deep copy

Kopya **öz massivini** almalıdır:

```
IntVector b = a;   (deep copy)

a.data_ ──► [ 1 | 2 | 3 | ? ]
b.data_ ──► [ 1 | 2 | 3 ]          ayrı massiv, ayrı sahib
```

## 3. Rule of three

> Sinif destructor, copy constructor və ya copy assignment-dən **birini** özü yazmalı olursa, çox güman ki **üçünü də** yazmalıdır.

Səbəbi: özünüz destructor yazırsınızsa, deməli sinif bir **resursa sahibdir** (burada heap yaddaşına). Kompilyatorun yaratdığı kopyalama isə sahibliyi bilmir. O, sadəcə pointer-i kopyalayır.

| # | Metod | Nə vaxt çağırılır | Bizdə nə edir |
|---|---|---|---|
| 1 | `~IntVector()` | obyekt dağıdılanda | massivi silir |
| 2 | `IntVector(const IntVector&)` | **yeni** obyekt kopyadan qurulanda | yeni massiv ayırır, elementləri köçürür |
| 3 | `operator=(const IntVector&)` | **mövcud** obyektə başqası verilir | köhnəni atır, kopyanı götürür |

A5-də bu siyahıya iki metod da əlavə olunacaq (move constructor və move assignment) və **rule of five** alınacaq.

### Hansı sətir hansını çağırır? (tələ!)

```cpp
IntVector b = a;    // COPY CONSTRUCTOR — "=" olsa da! b yeni yaranır
IntVector c(a);     // copy constructor
IntVector d;        // default constructor
d = a;              // COPY ASSIGNMENT — d artıq mövcuddur
```

Qayda belədir: **obyekt bu sətirdə yaranırsa, constructor işləyir. Artıq var idisə, assignment işləyir.** `=` işarəsinin olub-olmaması həlledici deyil.

## 4. Copy constructor sətir-sətir

```cpp
IntVector(const IntVector& other)
    : data_(other.size_ == 0 ? nullptr : new int[other.size_]),
      size_(other.size_),
      capacity_(other.size_) {
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}
```

- **Parametr `const IntVector&` (reference) olmalıdır.** Dəyərlə (`IntVector other`) olsaydı, arqumenti ötürmək üçün kopyalamaq lazım olardı. Kopyalamaq isə copy constructor-u çağırmaq deməkdir, yəni funksiya özünü sonsuz çağırardı. Kompilyator buna icazə vermir.
- **`const`**: mənbəni kopyalayırıq, dəyişmirik. `const` olmasa, `const` obyektləri və müvəqqəti obyektləri kopyalamaq olmazdı.
- **`other.data_`-ya necə çata bilirik, o private deyil?** C++-da `private` **sinif** səviyyəsindədir, obyekt səviyyəsində deyil. `IntVector`-un metodları istənilən `IntVector`-un private üzvlərini görür.
- **Boş vektor üçün `nullptr`**: invariant 2-ni qoruyur (`capacity_ == 0` olarsa, `data_ == nullptr`). `new int[0]` də qanunidir, amma lazımsız yaddaş ayırır.
- **`capacity_(other.size_)`**: kopya yalnız lazım olan qədər yer alır, artıq yer saxlamır. `std::vector` də praktikada belə edir. Test bunu yoxlayır: capacity 8, size 5 olan vektorun kopyasının capacity-si 5-dir.
- **Initializer list sırası**: üzvlər **class-da elan olunduğu sırada** init olunur (`data_`, `size_`, `capacity_`), initializer list-də yazıldığı sırada yox. Biz eyni sırada yazmışıq. Fərqli yazılsa, `-Wall` `-Wreorder` warning-i verir.

## 5. Copy assignment: copy-and-swap

```cpp
IntVector& operator=(const IntVector& other) {
    IntVector copy(other);   // 1. kopyanı müvəqqəti obyektdə qur
    swap(copy);              // 2. öz məzmunumuzu onunla dəyiş
    return *this;            // 3. copy funksiyadan çıxanda KÖHNƏ məzmunu silir
}

void swap(IntVector& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}
```

Addım-addım, `a = b;` üçün:

```
əvvəl:      a ──► [köhnə]          b ──► [1 2 3]
1. copy:    copy ──► [1 2 3]  (yeni massiv)
2. swap:    a ──► [1 2 3]          copy ──► [köhnə]
3. return:  copy dağıdılır → [köhnə] silinir
```

### Niyə belə, "əvvəl sil, sonra kopyala" yox?

Sadəlövh versiya belə olardı:

```cpp
IntVector& operator=(const IntVector& other) {
    delete[] data_;                       // köhnəni sil
    data_ = new int[other.size_];         // yenisini ayır
    /* ... kopyala ... */
    return *this;
}
```

Bu versiyanın iki ciddi problemi var:

1. **Self-assignment (`a = a`).** `delete[] data_` həm də `other.data_`-nı silir, çünki ikisi eyni obyektdir. Sonra silinmiş yaddaşdan kopyalayırıq. Bunu `if (this == &other) return *this;` ilə düzəltmək olar, amma ikinci problem qalır.
2. **Exception safety.** `new` yaddaş tapmasa, exception atır. Biz isə köhnə massivi **artıq silmişik**: `data_` silinmiş yaddaşı göstərir, obyekt pozulmuş vəziyyətdə qalır və destructor-u onu yenə siləcək.

Copy-and-swap ikisini də birdən həll edir:
- **Self-assignment:** əvvəl tam kopya alınır, sonra dəyişdirilir. Heç nə vaxtından əvvəl silinmir.
- **Exception safety:** riskli iş (`new`) yalnız 1-ci addımdadır, `*this`-ə hələ toxunulmayıb. Exception olsa, `a` əvvəlki kimi qalır. Buna **strong exception guarantee** deyilir: əməliyyat ya tam uğurlu olur, ya da heç nə dəyişmir.
- **Kod təkrarı yoxdur:** kopyalama məntiqi yalnız copy constructor-dadır.

Qiyməti: self-assignment zamanı lazımsız bir kopya alınır. Self-assignment çox nadir olduğu üçün bu, qəbul ediləndir.

### `swap` niyə `noexcept`?

`swap` yalnız üç ədədi (pointer və iki `size_t`) dəyişir. Burada yaddaş ayırmaq yoxdur, ona görə exception da ola bilməz. `noexcept` bunu kompilyatora və oxucuya bəyan edir. Copy-and-swap-ın zəmanəti də buna əsaslanır: 2-ci addım heç vaxt uğursuz olmur. A5-də `noexcept`-in vektor üçün daha böyük əhəmiyyətini görəcəyik (`std::move_if_noexcept`).

`swap` həm də O(1)-dir: neçə element olmasından asılı olmayaraq yalnız 3 dəyişən dəyişir.

### `operator=` niyə `IntVector&` qaytarır?

`a = b = c;` zəncirini mümkün etmək üçün. Bu, sağdan sola işləyir: `a = (b = c)`. `b = c` `b`-yə reference qaytarır, sonra `a`-ya o verilir. `int`-lər də belə işləyir, öz tiplərimiz də eyni davranmalıdır.

## 6. Testlər nəyi yoxlayır?

| Test | Nəyi tutur |
|---|---|
| copy has the same elements | Kopyalama ümumiyyətlə işləyir |
| copy is independent (deep copy) | Shallow copy-ni tutur: kopyanı dəyişmək orijinala təsir etməməlidir |
| copy of an empty vector | `nullptr` halı; boş kopya istifadəyə yararlıdır |
| copy does not keep spare capacity | Kopyanın `capacity == size` siyasəti |
| into a smaller and a larger vector | Assignment hər iki istiqamətdə ölçünü düzgün dəyişir; `SUBCASE` hər halı təmiz başlanğıcla işlədir |
| result is independent | Assignment-də də deep copy |
| self-assignment is safe | `a = a` məlumatı pozmur |
| chaining a = b = c | `operator=` reference qaytarır |
| swap exchanges contents | `swap` düzgün işləyir |

**Self-assignment testində niyə alias?** `a = a;` yazsaq, bəzi kompilyatorlar "özünə mənimsətmə" warning-i verir. Real proqramlarda self-assignment adətən dolayı yolla olur, məsələn, `v[i] = v[j]` (i == j olanda) və ya iki reference eyni obyekti göstərəndə. Alias bu real halı təqlid edir.

**SUBCASE nədir?** doctest `TEST_CASE`-i hər `SUBCASE` üçün **başdan** işə salır. Ümumi hazırlıq (`big`, `small`) bir dəfə yazılır, hər alt-hal isə təmiz vəziyyətdən başlayır.

## 7. Bilinən məhdudiyyətlər (növbəti addımlar)

- Hələ də yalnız `int` saxlayır. **A3** (template) bunu həll edir.
- Grow və copy hələ `new int[]` istifadə edir, bütün yeri init edir. **A4** bunu həll edir.
- Funksiyadan vektor qaytarmaq və ya müvəqqəti vektoru vermək hələ də kopyalama ola bilər. Müvəqqəti obyekt onsuz da silinəcək, onun massivini sadəcə "götürmək" olardı. **A5** (move semantics) bunu həll edir.

## 8. Müdafiədə gələ biləcək suallar

**Shallow copy ilə deep copy-nin fərqi nədir?** Shallow copy pointer-i kopyalayır, iki obyekt eyni yaddaşı paylaşır. Deep copy pointer-in göstərdiyi məlumatı kopyalayır, hər obyektin öz yaddaşı olur.

**Rule of three nədir və niyə lazımdır?** Sinif resursa sahibdirsə, destructor, copy constructor və copy assignment-i özü yazmalıdır. Kompilyatorun versiyaları pointer-i kopyalayır, nəticədə double free və asılı kopyalar alınır.

**`IntVector b = a;` hansı funksiyanı çağırır?** Copy constructor-u. `b` bu sətirdə yaranır, assignment isə yalnız artıq mövcud obyektə tətbiq olunur.

**Copy constructor-un parametri niyə reference-dir?** Dəyərlə ötürmək kopyalamaq tələb edərdi, kopyalamaq isə copy constructor-un özünü çağırmaq deməkdir. Bu, sonsuz rekursiyadır.

**Copy-and-swap nədir, nə üstünlüyü var?** Əvvəl müvəqqəti kopya qurulur, sonra `*this` onunla dəyişdirilir, köhnə məzmun müvəqqəti obyektlə birlikdə silinir. Self-assignment avtomatik təhlükəsiz olur və strong exception guarantee verir: `new` uğursuz olsa, obyekt dəyişmir.

**Double free niyə təhlükəlidir?** Undefined behavior-dur. Heap meneceri pozula bilər, proqram çökə bilər, və ya daha pisi, səssizcə səhv işləyə bilər. Təhlükəsizlik baxımından double free hücum vektoru da ola bilər.
