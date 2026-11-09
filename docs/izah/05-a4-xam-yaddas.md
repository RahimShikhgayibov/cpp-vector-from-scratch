# A4: v4 — Xam yaddaş və placement new

**Fayllar:**
- [`experiments/a4_raw_memory.cpp`](../../experiments/a4_raw_memory.cpp): `new T[]` modeli ilə yeni modelin müqayisəsi
- [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp): v4
- [`tests/test_types.hpp`](../../tests/test_types.hpp): `Tracked` sayğac tipi
- [`tests/test_vector_lifetime.cpp`](../../tests/test_vector_lifetime.cpp): ömür, `reserve`, `pop_back`, exception testləri

Bu, layihənin **ən vacib addımıdır**. Müdafiədə ilk sual çox güman ki budur: *"Niyə `new T[]` yox, `::operator new` + placement new?"*

## 1. Problem: `new T[]` iki işi birləşdirir

`new T[n]` eyni anda iki iş görür: **yaddaş ayırır** və orada **n obyekt qurur**. Vektora isə bunlar ayrı-ayrı lazımdır: `capacity` yer, amma yalnız `size` obyekt.

Nümayiş proqramı eyni əməliyyatları köhnə (v3) və yeni (v4) modeldə edir. Bu kompüterdəki həqiqi çıxış:

```
=== OldVector: new T[] model (v3) ===
      ctor    #1                      ← item (mənbə)
  1) reserve(3) on an empty vector:
      ctor    #2                      ← 3 LAZIMSIZ obyekt!
      ctor    #3
      ctor    #4
  2) push_back(item):
      assign  #2 <- #1                ← qurmaq yox, mövcud obyektin üstünə yazmaq
  3) pop_back():
                                      ← HEÇ NƏ: obyekt dağıdılmadı
  4) end of scope (vector destroyed):
      dtor    #4                      ← capacity qədər dtor, size qədər yox
      dtor    #3
      dtor    #2
  (source item destroyed:)
      dtor    #1

=== mystl::vector: raw memory model (v4) ===
      ctor    #5
  1) reserve(3) on an empty vector:
                                      ← heç bir obyekt qurulmur
  2) push_back(item):
      copy    #6 <- #5                ← obyekt məhz indi qurulur
  3) pop_back():
      dtor    #6                      ← obyekt dərhal dağıdılır
  4) end of scope (vector destroyed):
                                      ← canlı obyekt qalmayıb
  (source item destroyed:)
      dtor    #5
```

`new T[]` modelinin üç problemi:

| Problem | Nəticəsi |
|---|---|
| **1. Default constructor tələbi** | `NoDefault` kimi tiplər kompilyasiya olunmur (A3-də göstərdik: `no matching function for call to 'NoDefault::NoDefault()'`) |
| **2. `reserve` lazımsız obyektlər qurur** | Bahalı və ya yan təsirli constructor-lar boş yerə işləyir |
| **3. `pop_back` obyekti dağıda bilmir** | `delete[]` yalnız bütün massivi silir. "Silinmiş" element yaşamağa davam edir, tutduğu resurslar (məsələn, faylın açıq qalması və ya yaddaş) vektor dağılana qədər azad olunmur |

## 2. Əsas fikir: ayırmaq ≠ qurmaq

Adi `new T(args)` ifadəsi əslində **iki addımdır**. `delete p` də iki addımdır:

| İfadə | Addım 1 | Addım 2 |
|---|---|---|
| `new T(args)` | `::operator new(sizeof(T))`: **xam yaddaş** | constructor: o yaddaşda **obyekt qurur** |
| `delete p` | destructor: **obyekti dağıdır** | `::operator delete(p)`: **yaddaşı qaytarır** |

Vektor bu addımları **ayrı-ayrı** çağırır:

| Addım | Funksiya | Vektorda harada |
|---|---|---|
| Yaddaş al | `::operator new(bytes)` | `allocate()`: `reserve`, grow |
| Obyekt qur | placement new: `::new (p) T(args)` və ya `std::construct_at(p, args)` | `push_back` |
| Obyekti dağıt | `p->~T()` və ya `std::destroy_at(p)` | `pop_back`, destructor |
| Yaddaşı qaytar | `::operator delete(p)` | `deallocate()`: destructor, grow |

### Placement new

```cpp
void* raw = ::operator new(sizeof(T));   // xam yaddaş, obyekt yoxdur
T* p = ::new (raw) T(args);              // placement new: obyekti MƏHZ BU ünvanda qur
p->~T();                                 // destructor-u əl ilə çağır
::operator delete(raw);                  // yaddaşı qaytar
```

Placement new yaddaş **ayırmır**: verilən ünvanda yalnız constructor-u çağırır.

**`std::construct_at` (C++20)** placement new-un rahat formasıdır: `std::construct_at(p, args...)` daxildə `::new (static_cast<void*>(p)) T(args...)` edir. **`std::destroy_at(p)`** isə `p->~T()` edir. Kodda bunlardan istifadə edirik, çünki oxumaq daha asandır və `T`-nin adını yazmağa ehtiyac qalmır.

## 3. Yeni yaddaş modeli: `[qurulmuş | xam]`

```
size = 3, capacity = 5

data_ ──► [ e0 | e1 | e2 | ░░ | ░░ ]
           └ qurulmuş ┘   └ xam ─┘
```

**Yeni invariant:** `[0, size_)` yerlərində **dəqiq `size_` canlı obyekt** var, `[size_, capacity_)` isə **xamdır**: orada obyekt yoxdur.

Bu invariantdan iki vacib qayda çıxır:
- **Xam yerə `=` ilə yazmaq olmaz.** `data_[i] = x` mövcud obyektin `operator=`-unu çağırır, amma xam yerdə obyekt yoxdur. Bu, undefined behavior-dur. Xam yerdə obyekt **qurulur**: `std::construct_at`.
- **Xam yerdə destructor çağırmaq olmaz.** Destructor yalnız `[0, size_)` üçün çağırılır.

## 4. Kod hissə-hissə

### `allocate` / `deallocate`: xam yaddaş

```cpp
static T* allocate(size_type n) {
    if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
        throw std::bad_array_new_length();
    }
    void* raw = nullptr;
    if constexpr (over_aligned) {
        raw = ::operator new(n * sizeof(T), std::align_val_t{alignof(T)});
    } else {
        raw = ::operator new(n * sizeof(T));
    }
    return static_cast<T*>(raw);
}
```

- **Overflow yoxlaması.** `n * sizeof(T)` `size_t`-ni aşa bilər. Məsələn, `n` çox böyükdürsə və `sizeof(T) = 64`-dürsə, hasil sıfırın ətrafına "dolanır" və kiçik bir ədəd alınır. Onda kiçik yaddaş ayrılar, sonra onun hüdudlarından kənara yazılar. `new T[n]` bunu özü yoxlayır, biz isə əl ilə yoxlamalıyıq. Test bunu yoxlayır: `bad_array_new_length` atılır və `capacity` dəyişmir.
- **Hizalama (alignment).** Hər tipin `alignof(T)` tələbi var: obyekt bu ədədin bölünəni olan ünvanda durmalıdır. Adi `::operator new` yalnız `__STDCPP_DEFAULT_NEW_ALIGNMENT__` (x86-64-də 16 bayt) qədər zəmanət verir. `alignas(64)` kimi tiplər üçün C++17-də `std::align_val_t` qəbul edən xüsusi versiya var. `if constexpr` hansı versiyanın lazım olduğunu **kompilyasiya vaxtı** seçir. Test 64-bayt hizalı tipin ünvanını yoxlayır.
- `deallocate` də eyni şərtlə uyğun `::operator delete`-i çağırır. Ayırma və azad etmə **cüt** olmalıdır. `nullptr` ötürmək təhlükəsizdir.

### Destructor

```cpp
~vector() {
    std::destroy_n(data_, size_);   // yalnız canlı obyektlər
    deallocate(data_);              // sonra yaddaş
}
```

Sıra vacibdir: əvvəl obyektlər, sonra yaddaş. Xam hissədə destructor çağırılmır.

### `push_back`: sürətli yol

```cpp
if (size_ < capacity_) {
    std::construct_at(data_ + size_, value);  // ilk xam yerdə qur
    ++size_;
    return;
}
```

### `push_back`: grow yolu və **v3-də tapılan bug**

v3-də grow belə idi:

```cpp
// v3:
grow();                        // köhnə massivi delete[] edir
data_[size_] = value;          // value köhnə massivdədirsə, artıq SİLİNİB!
```

`v.push_back(v[0])` yazılanda `value` `v[0]`-ın reference-idir, yəni **köhnə buffer-dədir**. `grow()` köhnə buffer-i silir, sonra silinmiş yaddaşdan oxuyuruq: **use-after-free**.

Bu bug v3-də həqiqətən var idi. `std::string` ilə Windows-da təsadüfən düzgün nəticə alındı, çünki azad edilmiş yaddaş hələ köhnə məzmunu saxlayırdı. Amma destructor-u dəyəri "zəhərləyən" tiplə bug açıq görünür:

```
--- v3 ---
v[2].value = -1   (expected 42)     ← dağıdılmış obyektdən oxundu
--- v4 ---
v[2].value = 42   (expected 42)
```

Linux-da ASan bunu `heap-use-after-free` kimi göstərərdi. Yol xəritəsində bu hal "klassik tələ" kimi qeyd olunub.

**v4-də həll:** yeni elementi köhnə buffer dağıdılmamışdan **əvvəl** qururuq:

```cpp
const size_type new_capacity = next_capacity();
T* new_data = allocate(new_capacity);
try {
    std::construct_at(new_data + size_, value);       // 1. ƏVVƏL yeni element
} catch (...) {
    deallocate(new_data);
    throw;
}
try {
    std::uninitialized_copy_n(data_, size_, new_data); // 2. sonra köhnələri kopyala
} catch (...) {
    std::destroy_at(new_data + size_);
    deallocate(new_data);
    throw;
}
replace_buffer(new_data, new_capacity);               // 3. yalnız indi köhnəni at
++size_;
```

`libstdc++`-ın `std::vector`-u da eyni sıradan istifadə edir.

### `std::uninitialized_copy_n`

Kopyaları **xam** yaddaşda qurur, yəni hər element üçün copy constructor çağırır, `operator=` yox. Əsas xüsusiyyəti: **k-cı elementin kopyası exception atsa, artıq qurulmuş 0..k-1 kopyalarını özü dağıdır**, sonra exception-u ötürür. Bizə yalnız yaddaşı qaytarmaq qalır.

### `copy_to_new_buffer` və `replace_buffer`

```cpp
static T* copy_to_new_buffer(const T* first, size_type count, size_type capacity);
void replace_buffer(T* new_data, size_type new_capacity) noexcept;
```

`reserve`, copy constructor və grow eyni işi görür: yeni buffer qur, köhnəni at. Bu iş iki hissəyə bölünüb:
- **`copy_to_new_buffer`** uğursuz ola bilər, amma uğursuz olanda heç nəyə toxunmur.
- **`replace_buffer`** heç vaxt uğursuz olmur (`noexcept`). Köhnə obyektləri dağıdıb yeni buffer-ə keçir.

Bu bölgü strong exception guarantee-nin əsasıdır: **riskli iş əvvəl, geri dönüşü olmayan iş sonra**.

### `pop_back`

```cpp
void pop_back() {
    --size_;
    std::destroy_at(data_ + size_);
}
```

Obyekt dərhal dağıdılır, yer xam olur, `capacity` dəyişmir. Boş vektorda çağırmaq undefined behavior-dur: `std::vector`-da da belədir, yoxlama yoxdur. Bu, sürət üçün edilən seçimdir.

### `reserve`

```cpp
void reserve(size_type new_capacity) {
    if (new_capacity <= capacity_) return;     // kiçiltmir
    T* new_data = copy_to_new_buffer(data_, size_, new_capacity);
    replace_buffer(new_data, new_capacity);
}
```

Heç bir yeni obyekt qurmur, yalnız mövcudları köçürür. Test bunu yoxlayır: `reserve(100)`-dən sonra `Tracked::constructed == 0`.

## 5. Exception safety: strong guarantee

**Strong guarantee:** əməliyyat exception ilə bitsə, vektor **əməliyyatdan əvvəlki vəziyyətdə** qalır və heç nə sızmır.

| Əməliyyat | Necə təmin olunur |
|---|---|
| `push_back` (grow) | Yeni buffer tam hazır olana qədər köhnəyə toxunulmur. Hər `catch` öz qurduqlarını dağıdır və yaddaşı qaytarır |
| `reserve` | `copy_to_new_buffer` ya tam uğurlu olur, ya da heç nə buraxmır |
| copy constructor | Exception olsa, obyekt yaranmamış sayılır. `data_` hələ `nullptr`-dir, sızma yoxdur |
| copy assignment | Copy-and-swap (A2): kopya müvəqqəti obyektdə qurulur |

v3-də bu zəmanətlər **yox idi**: grow zamanı kopyalama exception atsa, `new_data` sızırdı.

### Testlər bunu necə yoxlayır? `Tracked`

[`tests/test_types.hpp`](../../tests/test_types.hpp)-dəki `Tracked`:
- Neçə dəfə qurulduğunu və dağıdıldığını sayır. `live() = constructed − destroyed`.
- Copy constructor-u **istənilən anda exception atmağa** proqramlaşdırmaq olur: `throw_on_copy_after = k` yazanda növbəti k kopya uğurlu olur, (k+1)-ci exception atır.
- Default constructor-u **yoxdur**: vektorun ondan asılı olmadığını da yoxlayır.

Windows-da ASan olmadığı üçün **sızmanı bu sayğac tutur**: test bitəndə `live() == 0` olmalıdır.

**Testin həqiqətən işlədiyini necə bilirik?** `push_back`-dəki `std::destroy_at(new_data + size_);` sətrini bilərəkdən silib testləri işlətdik:

```
TEST CASE:  exception in push_back during growth: vector is unchanged
test_vector_lifetime.cpp:213: ERROR: CHECK( Tracked::live() == live_before ) is NOT correct!
[doctest] test cases:   46 |   45 passed | 1 failed | 0 skipped
```

Test sızmanı (1 dağıdılmamış obyekt) tutdu. Sətir geri qaytarıldı. Bu üsula **mutation testing** deyilir: koda bilərəkdən səhv salıb testin onu tutduğunu yoxlayırsan.

## 6. Testlər

| Test | Nəyi yoxlayır |
|---|---|
| type without a default constructor works | Problem 1 həll olunub |
| reserve: creates no objects | Problem 2 həll olunub |
| reserve: smaller or equal value does nothing | `std::vector` davranışı |
| reserve: keeps existing elements | Köçürmədən sonra məlumat və `live()` düzgündür |
| reserve: no reallocation while size < capacity | `reserve`-dən sonra əlavə kopya yoxdur |
| pop_back: destroys exactly one object | Problem 3 həll olunub |
| pop_back then push_back reuses the slot | Xam yerdə yenidən qurmaq |
| every constructed object is destroyed exactly once | Bütün əməliyyatların qarışığından sonra balans |
| push_back(v[0]) when full (string və `Poisoned`) | v3-dəki use-after-free bug-ı |
| push_back(v.back()) many times | Eyni tələ, çoxlu grow ilə |
| exception in push_back / reserve / copy ctor / copy assignment | Strong guarantee və sızma yoxdur |
| over-aligned type | `alignas(64)` tipin ünvanı 64-ün bölünənidir |
| allocation size overflow | `bad_array_new_length`, vektor dəyişmir |

**Nəticə:** 46 test, 2223 yoxlama, hamısı keçir; build-də warning yoxdur.

## 7. Qalan məhdudiyyətlər (A5)

- **Grow hələ də kopyalayır.** `vector<std::string>` böyüyəndə hər sətir yenidən kopyalanır (yeni heap yaddaşı və simvolların kopyası), halbuki köhnə sətirlər dərhal silinəcək. Onların buffer-ini "götürmək" kifayət edərdi. → **move semantics**
- **`T` kopyalana bilməlidir.** `std::unique_ptr<int>` kopyalana bilmir, ona görə `vector<std::unique_ptr<int>>` hələ işləmir. → **A5**
- `T`-nin destructor-unun exception atmadığı fərz olunur. Standart kitabxana da bunu fərz edir.

## 8. Müdafiədə gələ biləcək suallar

**Niyə `new T[]` yox, `::operator new` + placement new?**
`new T[n]` yaddaş ayırmaqla obyekt qurmağı birləşdirir və capacity qədər obyekt qurur. Bunun üç nəticəsi var: default constructor tələb olunur, `reserve` lazımsız obyektlər qurur, `pop_back` obyekti dağıda bilmir. Ayrı-ayrı çağırış isə yaddaşı capacity qədər, obyektləri size qədər idarə etməyə imkan verir.

**Placement new nədir?**
Yaddaş ayırmadan, verilən ünvanda constructor çağıran `new` formasıdır: `::new (p) T(args)`. `std::construct_at` onun C++20 formasıdır.

**Destructor-u nə vaxt əl ilə çağırmaq lazımdır?**
Yalnız obyekt placement new ilə qurulubsa. Onda `delete` istifadə olunmur (yaddaş ayrıca idarə olunur), ona görə destructor ayrıca çağırılır: `std::destroy_at`. Adi obyektlər üçün əl ilə çağırmaq olmaz, çünki ikiqat dağıdılma baş verər.

**Xam yaddaşa niyə `=` ilə yazmaq olmaz?**
`operator=` mövcud obyektin metodudur və onun köhnə vəziyyətini oxuya bilər (məsələn, `std::string` köhnə buffer-ini). Xam yerdə obyekt yoxdur, ona görə bu, undefined behavior-dur. Obyekt qurulmalıdır: copy constructor ilə.

**`v.push_back(v[0])` niyə təhlükəlidir?**
Vektor dolu olanda grow köhnə buffer-i dağıdır. `v[0]` isə həmin buffer-dədir. Yeni element köhnə buffer dağıdılandan **sonra** qurulsa, silinmiş obyektdən oxunur. Həlli: yeni elementi əvvəl qurmaq. v3-də bu bug var idi, testlə göstərdik.

**Strong exception guarantee nədir və vektorunuz onu necə təmin edir?**
Əməliyyat ya tam uğurlu olur, ya da heç nə dəyişmir. Yeni buffer tam hazırlanır və riskli addımlar `try`-dadır. Köhnə buffer yalnız heç nə uğursuz ola bilməyəndə (`noexcept` `replace_buffer`) atılır. `Tracked` tipi ilə exception-ları süni yaradıb yoxlamışıq.

**Hizalama niyə vacibdir?**
Bəzi tiplər (SIMD, `alignas(64)`) müəyyən ünvan bölünənliyi tələb edir. Səhv hizalanmış obyektə müraciət yavaş işləyir və ya proqramı çökdürür. Adi `operator new` 16 bayta qədər zəmanət verir, ondan böyük tələblər üçün `std::align_val_t` versiyası istifadə olunur.
