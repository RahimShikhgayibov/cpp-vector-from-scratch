# mystl::vector — C++-da `std::vector` sıfırdan

C++20-də `std::vector`-un tam işlək analoqu. Layihə testlər, sanitizer-lər və benchmark ilə yoxlanılır.

## Vəziyyət

| Faza | Addım | Vəziyyət |
|---|---|---|
| 0 | Repo skeleti, CMake, doctest, ilk test | ✅ |
| A0 | Obyektin ömrü: `Noisy` təcrübəsi | ⏳ |
| A1 | v1: `IntVector` | ⏳ |
| A2 | v2: kopyalama, rule of three | ⏳ |
| A3 | v3: template `mystl::vector<T>` | ⏳ |
| A4 | v4: xam yaddaş, placement new | ⏳ |
| A5 | v5: move semantics, rule of five | ⏳ |
| B | Tam interfeys | ⏳ |
| C | Testlər, differential test, sanitizer-lər | ⏳ |
| D | Benchmark və 7 tədqiqat sualı | ⏳ |

## Struktur

```
include/mystl/   kitabxana (header-only)
tests/           doctest testləri
experiments/     Faza A təcrübələri
bench/           benchmark-lar
scripts/         qrafik skriptləri (Python)
docs/izah/       hər addımın ətraflı izahı
```

## Build

Tələblər: g++ 13+ (və ya clang 17+), CMake 3.20+, Ninja.

```
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Benchmark üçün `release` preset-i (`-O2`, sanitizer-siz) istifadə olunur.

**Sanitizer-lər:** Linux-da Debug build `-fsanitize=address,undefined` ilə işləyir. Windows MinGW-də ASan yoxdur, orada yalnız UBSan (trap rejimi) qoşulur. Tam yoxlama üçün WSL istifadə edin.

## İzahlar

- [Faza 0: repo və alətlər](docs/izah/00-faza0-repo-ve-aletler.md)
