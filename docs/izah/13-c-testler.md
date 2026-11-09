# Faza C: Testlər və sanitizer-lər

**Fayllar:**
- [`tests/`](../../tests): bütün testlər
- [`tests/differential.cpp`](../../tests/differential.cpp): differential test (ayrıca proqram)
- [`tests/test_edge_cases.cpp`](../../tests/test_edge_cases.cpp): yol xəritəsinin edge case siyahısı
- [`tests/test_types.hpp`](../../tests/test_types.hpp): sayğac və exception tipləri
- [`scripts/coverage.ps1`](../../scripts/coverage.ps1): coverage ölçümü

Yol xəritəsinə görə testlər Faza A və B-də hər metodla birlikdə yazılmalı idi. Belə də edildi. Faza C onları **tamamlayır**: differential test böyüdüldü, edge case siyahısı bir yerə yığıldı, coverage ölçüldü.

## 1. Test strategiyası: hər səviyyə başqa səhv tutur

| Səviyyə | Nə edir | Nəyi tutur |
|---|---|---|
| **Unit testlər** | hər metod ayrıca, əl ilə seçilmiş hallarla | məntiq səhvləri, səhv qaytarma dəyərləri |
| **Sayğac tipi** (`Tracked`) | qurulan/dağıdılan/kopyalanan/köçürülən obyektləri sayır | yaddaş sızması, ikiqat destructor, lazımsız kopyalar (Windows-da ASan əvəzi) |
| **Exception injection** | `throw_on_copy_after = k`: k-cı kopya exception atır | exception safety səhvləri, `catch`-də sızma |
| **Differential test** | 2 000 000 təsadüfi əməliyyat, `std::vector` ilə müqayisə | əl ilə düşünülməmiş ardıcıllıqlar |
| **Edge case-lər** | yol xəritəsinin siyahısı | sərhəd halları |
| **Mutation testing** | koda bilərəkdən səhv salıb testin onu tutduğunu yoxlamaq | **testlərin özünün** zəifliyi |
| **Coverage** | hansı sətirlərin heç icra olunmadığını göstərir | test olunmamış kod |
| **Sanitizer-lər** | işləmə vaxtı yaddaş və UB yoxlaması | use-after-free, buffer overflow, UB |

## 2. Rəqəmlər

| Fayl | Test case | Yoxlama | Mövzu |
|---|---|---|---|
| `test_main.cpp` | 1 | 1 | framework |
| `test_vector.cpp` | 15 | 2 095 | A1–A3: əsas məntiq, `string`, `vector<vector<int>>` |
| `test_vector_copy.cpp` | 12 | 45 | A2: rule of three |
| `test_vector_lifetime.cpp` | 18 | 87 | A4: obyektlərin ömrü, `reserve`, `pop_back`, exception, hizalama |
| `test_vector_move.cpp` | 16 | 257 | A5: move semantics, `unique_ptr` |
| `test_vector_access.cpp` | 17 | 56 | B1: giriş, iterator-lar |
| `test_vector_construct.cpp` | 21 | 74 | B2: constructor-lar, `assign` |
| `test_vector_capacity.cpp` | 15 | 56 | B3: tutum |
| `test_vector_modifiers.cpp` | 20 | 5 075 | B4: `insert`, `erase`, kiçik differential test |
| `test_vector_compare.cpp` | 13 | 45 | B5: müqayisə |
| `test_vector_professional.cpp` | 14 | 3 098 | B6: growth policy, 289 exception ssenarisi |
| `test_edge_cases.cpp` | 15 | 69 | C: edge case siyahısı, coverage boşluqları |
| **Cəmi (unit)** | **177** | **10 958** | işləmə vaxtı ~0.1 s |
| `differential.cpp` | 2 | 785 606 | C: 2 × 10 seed × 100 000 əməliyyat, ~6 s |

## 3. Test tipləri (yol xəritəsinin cədvəli)

| Tip | Nəyi yoxlayır | Harada |
|---|---|---|
| `int` | əsas məntiq | hər yerdə, differential test |
| `std::string` | heap-ə sahib obyektlərin düzgün kopyalanıb köçürülməsi | hər yerdə, differential test. **Uzun sətirlər** istifadə olunur ki, SSO səhvləri gizlətməsin |
| sayğac tipi (`Tracked`) | sızma, ikiqat destructor, lazımsız kopyalar | `test_types.hpp`; `live() == 0` hər testin sonunda |
| `std::unique_ptr<int>` | yalnız köçürülə bilən tiplər | `test_vector_move.cpp` |
| default constructor-u olmayan tip | xam yaddaş modelinin düzgünlüyü | `Tracked`-in default constructor-u yoxdur |
| N-ci kopyada exception atan tip | exception safety | `Tracked` və `TrackedThrowingMove`, `throw_on_copy_after` |

Əlavə tiplər: `Poisoned` (destructor dəyəri pozur, use-after-free-ni ASan-sız göstərir), `alignas(64) CacheLine` (hizalama), `OnlyLess` və `Unordered` (müqayisə), `InputOnly` (yalnız bir dəfə keçilən iterator).

## 4. Edge case-lər: yol xəritəsi → test

Hamısı [`test_edge_cases.cpp`](../../tests/test_edge_cases.cpp)-də yol xəritəsindəki ardıcıllıqla toplanıb (`tests.exe -tc="edge:*"`):

| Yol xəritəsindəki hal | Test |
|---|---|
| Boş vector: `at(0)` exception atır, `begin() == end()` | `edge: empty vector` |
| Self-assignment: `v = v;` | `edge: self-assignment v = v` (o cümlədən self-move) |
| `v.push_back(v[0]);` | `edge: v.push_back(v[0])`: dolu və yeri olan vektor |
| `v.insert(v.begin(), v.back());` | `edge: v.insert(v.begin(), v.back())`: yenidən ayırma ilə və onsuz |
| `insert`: əvvəldə, ortada, sonda, boş aralıq | `edge: insert at begin, middle, end, and an empty range` |
| `erase`: əvvəldə, ortada, sonda, boş aralıq | `edge: erase at begin, middle, end, and an empty range` |
| `reserve` cari ölçüdən kiçik dəyərlə | `edge: reserve with a value smaller than size and capacity` |
| `resize` yuxarı və aşağı | `edge: resize up and down` |
| `shrink_to_fit` boş vektorda | `edge: shrink_to_fit on empty vectors`: heç vaxt yaddaş ayırmamış və boşaldılmış |
| Move-dan sonra mənbənin vəziyyəti | `edge: moved-from vector is empty and usable` |

### Bu mərhələdə tapılan maraqlı hal: hesablanma sırası

Əvvəlcə belə yazılmışdı:

```cpp
CHECK(v.erase(v.end() - 1) == v.end());
```

Test **uğursuz oldu**, amma vektorda səhv yox idi. Səbəb: C++ `==` operatorunun iki tərəfinin **hansı sırada hesablanacağını təyin etmir**. GCC sağdakı `v.end()`-i `erase`-dən **əvvəl** hesabladı, yəni köhnə `end()`-i götürdü. `erase`-dən sonra həmin iterator **etibarsızdır**. Bu, iterator invalidation-un real nümunəsidir və test kodunda da baş verə bilir. Düzgün yazılış iki ayrı ifadədir:

```cpp
const auto after_last = v.erase(v.end() - 1);
CHECK(after_last == v.end());
```

### Bir fərq: `v.assign(v.begin(), v.end())`

`std::vector`-da bu çağırış **undefined behavior-dur**: standart deyir ki, iterator-lar `*this`-ə aid olmamalıdır. Bizim `assign` "qur, sonra swap" üsulu ilə işlədiyi üçün (B2) bu halda da düzgün işləyir. Test bunu göstərir.

## 5. Differential test

```
build/debug/differential.exe              # 10 seed × 100 000 addım, int və string
build/debug/differential.exe --steps=1000000 --seeds=3
```

**İdeya:** eyni təsadüfi əməliyyat ardıcıllığı həm `mystl::vector`, həm də `std::vector` üzərində icra olunur və **hər addımdan sonra** nəticələr müqayisə edilir. `std::vector` etalon kimi götürülür: fərq varsa, səhv bizdədir.

**30 əməliyyat:** bütün `push_back`/`emplace_back`/`insert`/`emplace` formaları, `pop_back`, `erase` (mövqe, aralıq, dəyər), `resize` (iki forma), `reserve`, `shrink_to_fit`, `clear`, `assign` (üç forma), copy constructor/assignment, move assignment, `swap`, alias halları (`push_back(v[i])`, `insert(pos, v[i])`, `resize(n, v[i])`), `at`/`front`/`back`, müqayisə operatorları.

**Hər addımdan sonra yoxlanılanlar:** ölçü, `empty()`, `size <= capacity` invariantı, `end - begin == size`, bütün elementlər. İki vektor cütü saxlanılır (`swap`, copy, move və müqayisə üçün), ikisi də yoxlanılır.

**Nəticə (həqiqi çıxış):**

```
1000000 operations (10 seeds x 100000), largest size 86, every operation used at least 33050 times
1000000 operations (10 seeds x 100000), largest size 88, every operation used at least 32926 times
[doctest] test cases:      2 |      2 passed | 0 failed | 0 skipped
[doctest] assertions: 785606 | 785606 passed | 0 failed |
```

Yol xəritəsinin şərti belədir: *"differential test 10 müxtəlif seed ilə keçir"*. Ödənilib, həm `int`, həm `std::string` üçün.

**Niyə ölçü kiçik saxlanılır (≤ 300)?** Hər addımdan sonra bütün elementlər müqayisə olunur. Böyük vektorda bu, çox yavaş olardı. Kiçik ölçülər həm də **kənar halları** (boş vektor, bir element, dolu vektor) daha tez-tez yaradır.

**Səhv olanda** test dəqiq yeri göstərir. Yoxlamaq üçün `emplace`-ə bilərəkdən səhv salındı: sondan əvvəlki mövqeyə əlavə edəndə sürüşdürmə buraxıldı. Nəticə:

```
mismatch with std::vector: seed = 1, step = 254, operation = insert(move)
mismatch with std::vector: seed = 1, step = 46, operation = emplace
```

Seed və addım nömrəsi məlumdur, ona görə səhvi **təkrar yaratmaq** asandır. Sonra səhv geri qaytarıldı.

## 6. Mutation testing: layihə boyunca

Test keçəndə iki ehtimal var: ya kod düzgündür, ya da test zəifdir. Bunu yoxlamaq üçün koda **bilərəkdən** səhv salınıb testlərin onu tutub-tutmadığına baxılıb:

| Addım | Salınan səhv | Nəticə |
|---|---|---|
| A4 | `push_back`-in `catch`-ində yeni elementin `destroy_at`-ı silindi | 1 test: `live()` 1 artıq (sızma) |
| A5 | `std::move_if_noexcept` → `std::move` | 3 test |
| A5 | vektorun move constructor-undan `noexcept` silindi | 2 test (o cümlədən daxili vektorların dərin kopyalanması) |
| B2 | constructor-dan `: vector()` delegation-u silindi | 2 test: 3 obyekt sızdı |
| B4 | `reallocate_with_tail`-də sıra dəyişdirildi (köhnələr əvvəl köçürülür) | 9 test: bütün alias halları |
| C | `emplace`-də sondan əvvəlki mövqedə sürüşdürmə buraxıldı | 3 unit test + differential test (`seed = 1, step = 46`) |

Hər səhv tutuldu və kod geri qaytarıldı.

## 7. Coverage

```
powershell -ExecutionPolicy Bypass -File scripts/coverage.ps1
```

Skript bütün testləri `--coverage` ilə kompilyasiya edir, işə salır və `gcov` ilə `vector.hpp`-nin hansı sətirlərinin icra olunduğunu sayır. Header bir neçə `.cpp`-yə daxil olduğu üçün nəticələr birləşdirilir: sətir hər hansı bir faylda icra olunubsa, əhatə olunmuş sayılır.

| | Sətir | Faiz |
|---|---|---|
| İlk ölçüm | 288 / 299 | 96.3% |
| Boşluqlar üçün test əlavə edildikdən sonra | **296 / 299** | **99.0%** |

İlk ölçüm **test olunmamış 8 sətir** göstərdi. Onlar üçün yeni testlər yazıldı:
- input iterator ilə `insert` zamanı exception: əlavə olunanlar geri alınır (`InputOnly` adapteri ilə);
- `shrink_to_fit` zamanı exception: vektor dəyişmir;
- `insert(pos, n, value)` və `resize(n)` `max_size`-ı aşanda `length_error` atılır.

**Qalan 3 sətir** müdafiə xarakterlidir və praktikada test oluna bilməz: hamısı `capacity >= max_size() / 2` (≈ 4.6 · 10¹⁸ bayt) və ya artıq yoxlanılmış overflow halları üçündür. Kodda saxlanılır, çünki ictimai interfeysdə yoxlama dəyişsə, növbəti müdafiə xətti onlardır.

**Məhdudiyyət:** template kodu yalnız instansiya olunduqda sətir sayılır. Heç bir testdə çağırılmayan metod hesabata ümumiyyətlə düşmür. Ona görə 99% "bütün kodun 99%-i" yox, "instansiya olunan kodun 99%-i" deməkdir. Bu riski azaltmaq üçün B6-da hər `const` metodun çağırıldığı ayrıca test var.

## 8. Testlərin tapdığı səhvlər

Testlər yalnız "keçmək" üçün yazılmayıb, layihə boyunca real səhvlər tapıblar:

| Harada | Səhv | Necə tapıldı |
|---|---|---|
| v3 (A3) kodu | dolu vektorda `push_back(v[0])`: köhnə massiv silinəndən **sonra** oxunurdu (use-after-free) | A4-də `Poisoned` tipi: gözlənilən 42 əvəzinə −1 |
| A1 izahı | "n əməliyyatda n-dən az köçürmə" iddiası yalnız 2-nin qüvvətləri üçün doğrudur | B6-da sayğac: n = 10 000 üçün 16 383 köçürmə. Düzgün hədd `< 2n` |
| Test kodu (B6) | köçürmə sayı və `const` testində səhv gözlənti | testin özü uğursuz oldu, hesab yenidən yoxlandı |
| Test kodu (C) | `==`-nin hesablanma sırasından asılı iterator müqayisəsi | yuxarıda, bölmə 4 |

## 9. Sanitizer-lər

| Platforma | Sanitizer | Vəziyyət |
|---|---|---|
| Windows (MinGW g++ 16.2) | UBSan, trap rejimi (`-fsanitize=undefined -fsanitize-trap=all`) | ✅ bütün Debug build-lər bununla işləyir, 177 unit test + differential test keçir |
| Linux (Docker, `gcc:14`, g++ 14.4.0) | ASan + UBSan + LeakSanitizer (`-fsanitize=address,undefined`) | ✅ **təmiz** |

Windows-da ASan olmadığı üçün onun əsas işini əvvəlcə başqa vasitələr görürdü:
- **sızma və ikiqat destructor:** `Tracked::live()` sayğacı;
- **use-after-free:** `Poisoned` tipi (destructor dəyəri pozur).

Amma bu vasitələr ASan-ı tam **əvəz etmir**: ASan hər yaddaş müraciətini yoxlayır, sayğac isə yalnız obyektlərin sayını izləyir. Ona görə yol xəritəsinin "ASan təmizdir" şərti Linux-da yoxlanıldı.

### Linux-da ASan: necə və nə alındı

```
docker run --rm --cap-add=SYS_PTRACE -v "${PWD}:/src" -w /src gcc:14 bash scripts/asan_linux.sh
```

[`scripts/asan_linux.sh`](../../scripts/asan_linux.sh) bütün testləri `-O0 -g -fsanitize=address,undefined -fno-sanitize-recover=all` ilə kompilyasiya edib işə salır. `-fno-sanitize-recover=all` olduğu üçün ilk xətada proqram dayanır və test uğursuz sayılır. Həqiqi çıxış:

```
=== g++ (GCC) 14.4.0
=== unit tests
[doctest] test cases:   177 |   177 passed | 0 failed | 0 skipped
[doctest] assertions: 10958 | 10958 passed | 0 failed |
=== differential test (10 seeds x 100000 steps)
[doctest] test cases:      2 |      2 passed | 0 failed | 0 skipped
[doctest] assertions: 785606 | 785606 passed | 0 failed |
=== experiments that must be clean (a4, a5)
a4_raw_memory: clean
a5_move_semantics: clean
=== experiments with intentional bugs (ASan must report them)
a0_noisy (new without delete): LeakSanitizer: detected memory leaks
a2_shallow_copy (shallow copy): AddressSanitizer: attempting double-free
=== RESULT: all sanitizer checks passed
```

**Nəzarət yoxlaması (positive control):** sanitizer heç nə tapmayanda iki izah mümkündür: ya kod təmizdir, ya da sanitizer işləmir. Bunu ayırd etmək üçün **bilərəkdən səhv** olan iki təcrübə proqramı da işə salınır:
- A0-dakı `delete`-siz `new` → `LeakSanitizer: detected memory leaks`;
- A2-dəki shallow copy → `AddressSanitizer: attempting double-free`.

İkisi də tutuldu, deməli sanitizer həqiqətən işləyir. Testlərin təmiz çıxması da etibarlıdır.

**İkinci kompilyator:** GCC 14.4 ilə bütün mənbə faylları `-Wall -Wextra -Wpedantic` ilə **0 warning** verir. Windows-dakı GCC 16.2 ilə də 0 warning var.

## 10. Müdafiədə gələ biləcək suallar

**Testləriniz düzgün olduğunu necə bilirsiniz?**
Mutation testing ilə: koda bilərəkdən 6 fərqli səhv salındı və hər birini ən azı bir test tutdu. Coverage 99%-dir, qalan 3 sətir isə müdafiə xarakterlidir.

**Differential test nədir və niyə lazımdır?**
Eyni təsadüfi əməliyyatları bizim vektorda və `std::vector`-da icra edib hər addımdan sonra nəticələri müqayisə etmək. Əl ilə ağla gəlməyən ardıcıllıqları yoxlayır. 10 seed × 100 000 addım × 2 tip = 2 milyon əməliyyat, hamısı uyğun gəlir.

**Yaddaş səhvlərinin olmadığını necə bilirsiniz?**
İki səviyyədə yoxlanılıb. Windows-da (ASan yoxdur) obyektləri sayan test tipi var: hər testin sonunda canlı obyektlərin sayı 0 olmalıdır. Linux-da isə bütün testlər və differential test AddressSanitizer + LeakSanitizer + UBSan ilə təmiz keçir. Sanitizer-in həqiqətən işlədiyi bilərəkdən səhv yazılmış iki proqramla yoxlanılıb: ikisini də tutdu.

**Exception safety-ni necə test etdiniz?**
Test tipinin copy constructor-u istənilən k-cı çağırışda exception atır. `push_back` və `reserve` üçün hər mümkün k yoxlanılır: cəmi 289 ssenari. Hər birindən sonra vektor dəyişməyib və sızma yoxdur.
