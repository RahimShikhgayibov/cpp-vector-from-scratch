# Təhvil siyahısı (sillabus, "Qeyd 4")

| # | Sənəd | Fayl |
|---|---|---|
| 1 | Layihə təklifi (Project Proposal) | [`docs/report/layihe-teklifi.pdf`](report/layihe-teklifi.pdf) (HTML mənbəyi yanındadır) |
| 2 | Texniki hesabat | [`docs/report/hesabat.pdf`](report/hesabat.pdf), 8 səhifə |
| 3 | Source code və Git repository linki | https://github.com/RahimShikhgayibov/cpp-vector-from-scratch |
| 4 | Mürəkkəblik analizi | hesabat, bölmə 4: bütün metodların cədvəli, `push_back` üçün amortized O(1) isbatı (aggregate və accounting metodları) |
| 5 | Test ssenariləri və nəticələri | [`docs/izah/13-c-testler.md`](izah/13-c-testler.md); hesabat, bölmə 5 |
| 6 | Benchmark nəticələri və qrafiklər | [`docs/izah/14-d-benchmark.md`](izah/14-d-benchmark.md), [`docs/benchmark/`](benchmark) (`q1.png` … `q7.png`, `summary.md`) |
| 7 | Test input-ları (benchmark parametrləri və seed-lər) | aşağıda; xam nəticələr: [`docs/benchmark/data/`](benchmark/data) |
| 8 | README və işə salma təlimatı | [`README.md`](../README.md) |
| 9 | Təqdimat (PPTX və ya PDF) | [`docs/report/teqdimat.pdf`](report/teqdimat.pdf), 15 slayd |
| 10 | İstinadlar | hesabatın sonunda |

PDF-lər HTML mənbələrindən yaradılır:

```
powershell -ExecutionPolicy Bypass -File scripts/make_pdf.ps1 docs/report/hesabat.html docs/report/teqdimat.html docs/report/layihe-teklifi.html
```

## Test input-ları

### Differential test (`tests/differential.cpp`)

| Parametr | Dəyər |
|---|---|
| Təsadüfi ədəd generatoru | `std::mt19937` |
| Seed-lər | 1, 2, …, 10 |
| Addım sayı (seed başına) | 100 000 |
| Element tipləri | `int` (0…99), `std::string` (yarısı qısa `"s<n>"`, yarısı ~55 simvolluq) |
| Əməliyyat sayı | 30 növ, hər biri ≥ 32 926 dəfə |
| Maksimum ölçü | 300 (aşdıqda yarısı silinir) |
| İşə salma | `build/debug/differential.exe [--steps=N] [--seeds=K]` |

### Unit testlərdəki təsadüfi test (`tests/test_vector_modifiers.cpp`)

`std::mt19937`, seed `12345`, 5 000 addım.

### Exception injection (`tests/test_vector_professional.cpp`)

`push_back`: vektor ölçüsü 0…16, exception k = 0…ölçü (153 ssenari). `reserve`: ölçü 1…16, k = 0…ölçü−1 (136 ssenari).

### Benchmark (`bench/benchmarks.cpp`)

| Parametr | Dəyər |
|---|---|
| Build | Release, `-std=c++20 -O2`, sanitizer-siz |
| Təkrar | 1 ısınma + 9 ölçmə, median (`--repeats=9`) |
| n dəyərləri | 10³ … 10⁷, onluq başına 1, 2, 5 (Sual 4: 10⁶-ya qədər) |
| Sual 4 sətri | 40 simvol (SSO-ya sığmır) |
| Sual 5 | `reserve(n + ops)` edilmiş `int` vektoru; ops = 200 (n < 10⁶), 20 (n ≥ 10⁶) |
| Sual 6 | qarışıq list üçün `std::mt19937` seed 42 |
| Sual 8 | Array: `FixedArray` (sabit tutum); axtarış açarları `std::mt19937` seed 7; xətti axtarış 200 açar (n ≥ 10⁶: 20), binar axtarış 10 000 açar |
| Avadanlıq | AMD Ryzen 7 PRO 5850U, 31 GB RAM, Windows 11 Pro 23H2, g++ 16.2.0 |
| İşə salma | `build/release/benchmarks.exe --repeats=9`, sonra `python scripts/plot_benchmarks.py` |

### Sanitizer yoxlaması

`docker run --rm --cap-add=SYS_PTRACE -v "${PWD}:/src" -w /src gcc:14 bash scripts/asan_linux.sh` (g++ 14.4.0).
