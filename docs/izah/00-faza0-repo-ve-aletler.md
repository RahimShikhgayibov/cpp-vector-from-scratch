# Faza 0: Repo və alətlər

**Nəticə:** build olunan layihə və keçən ilk test. Bu, 4-cü həftədə göstərilən "repository skeleton"-dur.

## 1. Qovluq strukturu

```
DSA FF/
├── include/mystl/      kitabxananın özü (yalnız .hpp faylları)
├── tests/              testlər + doctest.h
├── experiments/        Faza A-nın kiçik təcrübə proqramları (məs. Noisy)
├── bench/              benchmark proqramları (Faza D)
├── scripts/            Python qrafik skriptləri (Faza D)
├── docs/               hesabat, qrafiklər, slaydlar, bu izahlar
├── CMakeLists.txt      build qaydaları
├── CMakePresets.json   hazır build rejimləri (debug / release)
├── README.md
└── .gitignore
```

**Niyə `include/mystl/`, sadəcə `include/` yox?** Testdə `#include <mystl/int_vector.hpp>` yazırıq. `mystl/` prefiksi adların toqquşmasının qarşısını alır. Məsələn, başqa kitabxanada da `vector.hpp` ola bilər. `std::` namespace-i kimi, bizim kodumuz da `mystl::` namespace-indədir.

## 2. CMake nədir?

CMake özü kompilyasiya etmir. O, **build sistemi yaradır**: `CMakeLists.txt`-i oxuyur və Ninja (və ya Make) üçün fayllar yazır. Kompilyasiyanı Ninja `g++`-ı çağıraraq edir.

Bunun faydası odur ki, eyni `CMakeLists.txt` Windows-da, Linux-da və müəllimin kompüterində işləyir. Uzun `g++ ... ... ...` əmrlərini əl ilə yazmağa ehtiyac qalmır.

### `CMakeLists.txt` sətir-sətir

| Hissə | Nə edir |
|---|---|
| `cmake_minimum_required(VERSION 3.20)` | Köhnə CMake ilə açılsa, aydın xəta verir |
| `project(mystl LANGUAGES CXX)` | Layihənin adı; yalnız C++ istifadə olunur |
| `CMAKE_CXX_STANDARD 20` | `-std=c++20` |
| `CMAKE_CXX_STANDARD_REQUIRED ON` | Kompilyator C++20-ni bilmirsə, səssizcə köhnə standarta düşməsin, xəta versin |
| `CMAKE_CXX_EXTENSIONS OFF` | `gnu++20` yox, təmiz `c++20`. Kod yalnız GCC-yə xas genişlənmələrə bağlanmır |
| `add_library(mystl INTERFACE)` | **Header-only kitabxana.** Kompilyasiya olunan heç nə yoxdur. Sadəcə "məndən istifadə edən `include/`-u görsün" qaydasını daşıyır |
| `target_include_directories(mystl INTERFACE ...)` | `mystl`-ə bağlanan hər target `-I include` alır |
| `mystl_warnings` | Warning flag-lərini bir yerə yığan "paket". Hər target-ə ayrıca yazmırıq |
| `mystl_sanitizers` | Sanitizer flag-ləri, yalnız Debug-da |
| `$<$<CONFIG:Debug>:...>` | **Generator expression**: "yalnız Debug build-də bu flag-ləri əlavə et" |
| `enable_testing()` + `add_test(...)` | `ctest` əmri testləri tapıb işə salsın |

**`target_*` niyə?** Müasir CMake-də flag-lər qlobal yox, konkret target-ə (proqrama, kitabxanaya) verilir. Belə olanda `tests` sanitizer-li, gələcəkdə `bench` isə sanitizer-siz build oluna bilir.

### `CMakePresets.json`

Uzun konfiqurasiya əmrlərini adla çağırmaq üçündür:

| Preset | Qovluq | Flag-lər | Nə üçün |
|---|---|---|---|
| `debug` | `build/debug` | `-O0 -g`, warning-lər, sanitizer-lər | Səhv tapmaq, testlər |
| `release` | `build/release` | `-O2`, sanitizer-siz | **Yalnız benchmark** |

**Niyə benchmark Debug-da olmaz?** Debug-da optimallaşdırma yoxdur və sanitizer-lər hər yaddaş müraciətini yoxlayır. Belə olanda proqram 2–10 dəfə yavaş işləyir və ölçülən vaxt real kodu göstərmir.

## 3. Warning flag-ləri

| Flag | Mənası |
|---|---|
| `-Wall` | Ən çox rast gəlinən şübhəli kodlar: istifadə olunmayan dəyişən, init edilməmiş dəyişən və s. |
| `-Wextra` | Əlavə yoxlamalar: signed/unsigned müqayisəsi, istifadə olunmayan parametr və s. |
| `-Wpedantic` | Standart C++-da olmayan, yalnız GCC-nin qəbul etdiyi kod |

Yol xəritəsinin qaydası belədir: **addım warning-siz build olunmayıbsa, bitmiş sayılmır.** Warning çox vaxt gələcək bug-ın ilk əlamətidir.

## 4. Sanitizer-lər

Sanitizer, kompilyatorun proqrama əlavə etdiyi yoxlayıcı koddur. Proqram işləyərkən səhvi **baş verdiyi anda** tutur.

| Sanitizer | Nəyi tutur |
|---|---|
| **AddressSanitizer (ASan)** | massivdən kənara yazmaq, silinmiş yaddaşa müraciət (use-after-free), **double free**, yaddaş sızması (LeakSanitizer vasitəsilə) |
| **UndefinedBehaviorSanitizer (UBSan)** | signed overflow, null pointer-dən oxumaq, səhv shift və s. |

### Windows məhdudiyyəti

Bu kompüterdəki MinGW g++-da (WinLibs 16.2) `libasan` və `libubsan` kitabxanaları **yoxdur**. Bunu yoxlamışıq: `-fsanitize=address` ilə link etmək `cannot find -lasan` xətası verir. Ona görə:

- **Windows-da:** UBSan "trap" rejimində işləyir (`-fsanitize=undefined -fsanitize-trap=all`). UB baş verəndə proqram dərhal dayanır, amma ətraflı mesaj vermir.
- **Linux / WSL-də:** tam `-fsanitize=address,undefined` işləyir. CMake bunu özü seçir (`if(MINGW)`).

Yol xəritəsindəki "ASan təmizdir" şərti üçün testləri Linux-da (WSL Ubuntu) da işlətmək lazımdır. Quraşdırma (bir dəfəlik, administrator icazəsi istəyə bilər):

```
wsl --install -d Ubuntu
```

Sonra Ubuntu-da: `sudo apt install g++ cmake ninja-build`, layihə qovluğuna keç (`cd "/mnt/c/Users/user/Desktop/portfolio/DSA FF"`) və aşağıdakı əmrləri işlət. Linux build-ini ayrıca qovluqda saxlamaq üçün `--preset` yerinə `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug` yazmaq daha yaxşıdır.

## 5. doctest

**doctest** tək header faylından ibarət test framework-dür (`tests/doctest.h`, v2.4.11, rəsmi GitHub release-dən götürülüb). Quraşdırılmır, sadəcə include olunur.

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN   // yalnız test_main.cpp-də!
#include "doctest.h"

TEST_CASE("test framework works") {
    CHECK(1 + 1 == 2);
}
```

- `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`: doctest-ə `main()` funksiyasını yaratmağı deyir. **Yalnız bir faylda** olmalıdır, yoxsa iki `main()` alınır və link xətası çıxır.
- `CHECK(x)`: yoxlayır; səhvdirsə, qeyd edir və **davam edir**.
- `REQUIRE(x)`: yoxlayır; səhvdirsə, **bu test case-i dayandırır**. Sonrakı yoxlamalar ondan asılı olanda istifadə olunur. Məsələn, `REQUIRE(v.size() == 3)` keçməsə, `v[2]`-yə baxmaq təhlükəlidir.

## 6. Əmrlər

```
cmake --preset debug           # konfiqurasiya (bir dəfə, və ya CMakeLists dəyişəndə)
cmake --build --preset debug   # kompilyasiya
ctest --preset debug           # testlər
build/debug/tests.exe          # testləri birbaşa, ətraflı çıxışla
```

## 7. Müdafiədə gələ biləcək suallar

**Niyə header-only?** Kitabxana template olacaq (`vector<T>`). Kompilyator template kodunu hər `T` üçün ayrıca yaradır, ona görə tam kodu görməlidir. Bu səbəbdən template-lər header-də yaşayır. Ətraflı izah A3-dədir.

**Niyə CMake, sadəcə `g++` əmri yox?** Layihədə bir neçə proqram var (testlər, təcrübələr, benchmark), iki build rejimi var və platformadan asılı flag-lər var. CMake bunları bir yerdə, platformadan asılı olmayan formada təsvir edir.

**Niyə Debug və Release ayrıdır?** Debug səhv tapmaq üçündür (sanitizer, debug info). Release ölçmək üçündür (`-O2`). Bir build ikisini birdən yaxşı edə bilmir.

**ASan Windows-da niyə yoxdur?** MinGW toolchain-i GCC-nin sanitizer runtime kitabxanalarını Windows üçün daşımır. Ona görə tam yoxlama Linux/WSL-də aparılır.
