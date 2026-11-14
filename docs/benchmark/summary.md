# Benchmark nəticələri (avtomatik yaradılıb: scripts/plot_benchmarks.py)

## Sual 1: push_back amortized O(1)-dirmi?

| n | mystl ns/op | std ns/op | köçürmə / n (mystl) |
|---|---|---|---|
| 1,000 | 1.50 | 1.40 | 1.023 |
| 10,000 | 4.21 | 5.47 | 1.638 |
| 100,000 | 3.35 | 4.47 | 1.311 |
| 1,000,000 | 1.93 | 1.78 | 1.049 |
| 10,000,000 | 2.41 | 2.28 | 1.678 |

## Sual 2: growth factor 2× və 1.5×

| siyasət | n | ns/op | yenidən ayırma | köçürmə / n | boş yer |
|---|---|---|---|---|---|
| 2x | 1,000,000 | 2.04 | 21 | 1.05 | 4.6% |
| 2x | 10,000,000 | 2.43 | 25 | 1.68 | 40.4% |
| 1.5x | 1,000,000 | 2.76 | 35 | 2.10 | 4.8% |
| 1.5x | 10,000,000 | 3.05 | 41 | 2.39 | 16.4% |

| siyasət | ölçülər | orta boş yer | maksimum boş yer |
|---|---|---|---|
| 2x | 1 … 1,000,000 | 26.1% | 50.0% |
| 1.5x | 1 … 1,000,000 | 17.4% | 33.3% |

## Sual 3: reserve nə qədər qazandırır?

| n | mystl reserve-siz | mystl reserve | sürətlənmə | std reserve-siz | std reserve | sürətlənmə |
|---|---|---|---|---|---|---|
| 10,000 | 0.65 | 0.34 | 1.91× | 1.94 | 0.28 | 6.93× |
| 100,000 | 3.04 | 0.32 | 9.52× | 3.74 | 0.27 | 14.00× |
| 1,000,000 | 1.49 | 0.84 | 1.77× | 1.67 | 0.84 | 1.99× |
| 10,000,000 | 2.37 | 0.93 | 2.53× | 1.96 | 0.93 | 2.10× |

## Sual 4: copy ilə move; noexcept unudulanda

| n | impl | move noexcept | noexcept YOX | fərq | push_back(copy) | push_back(move) | fərq |
|---|---|---|---|---|---|---|---|
| 10,000 | mystl | 85.7 | 156.2 | 1.82× | 55.1 | 20.6 | 2.67× |
| 10,000 | std | 80.1 | 179.6 | 2.24× | 56.0 | 23.0 | 2.43× |
| 100,000 | mystl | 111.9 | 210.8 | 1.88× | 95.2 | 47.2 | 2.02× |
| 100,000 | std | 87.8 | 162.5 | 1.85× | 101.5 | 46.6 | 2.18× |
| 1,000,000 | mystl | 109.0 | 207.4 | 1.90× | 131.8 | 53.2 | 2.48× |
| 1,000,000 | std | 98.2 | 199.9 | 2.04× | 131.9 | 54.6 | 2.42× |

## Sual 5: ortaya insert/erase nə qədər bahadır?

| n | əməliyyat | əvvəl (mystl / std) | orta (mystl / std) | son (mystl / std) |
|---|---|---|---|---|
| 1,000 | insert | 74 / 73 ns | 37 / 36 ns | 2 / 0 ns |
| 1,000 | erase | 39 / 36 ns | 28 / 21 ns | 0 / 1 ns |
| 100,000 | insert | 8,500 / 8,168 ns | 3,710 / 3,915 ns | 2 / 0 ns |
| 100,000 | erase | 6,116 / 6,440 ns | 3,113 / 3,105 ns | 0 / 1 ns |
| 10,000,000 | insert | 3,756,040 / 3,665,210 ns | 1,612,910 / 1,670,310 ns | 20 / 15 ns |
| 10,000,000 | erase | 4,061,940 / 4,047,950 ns | 1,680,070 / 1,682,170 ns | 15 / 20 ns |

## Sual 6: cache-in təsiri

| n | mystl::vector | std::vector | list (ardıcıl düyünlər) | list (qarışıq düyünlər) | qarışıq list / vector |
|---|---|---|---|---|---|
| 1,000 | 0.200 ns | 0.200 ns | 1.900 ns | 2.100 ns | 10× |
| 100,000 | 0.130 ns | 0.183 ns | 2.151 ns | 13.359 ns | 103× |
| 1,000,000 | 0.174 ns | 0.132 ns | 7.806 ns | 109.305 ns | 628× |
| 10,000,000 | 0.243 ns | 0.220 ns | 8.075 ns | 126.783 ns | 522× |

## Sual 7: mystl::vector std::vector-a nə qədər yaxındır?

Nisbət = mystl vaxtı / std vaxtı (1.00 = eyni). Hər əməliyyat üçün n ≥ 10 000 olan bütün ölçmələrin həndəsi ortası.

| əməliyyat | həndəsi orta | min | max | ölçmə sayı |
|---|---|---|---|---|
| push_back (int) | 0.84 | 0.43 | 1.19 | 10 |
| reserve + push_back | 1.02 | 0.60 | 1.21 | 10 |
| emplace_back (string) | 1.13 | 1.05 | 1.27 | 7 |
| push_back(copy) string | 1.04 | 0.94 | 1.27 | 7 |
| push_back(move) string | 1.00 | 0.90 | 1.18 | 7 |
| insert (orta) | 1.00 | 0.95 | 1.04 | 10 |
| erase (orta) | 0.99 | 0.93 | 1.02 | 10 |
| traversal | 1.03 | 0.71 | 1.32 | 10 |

## Sual 8: Array və Dynamic Array

| n | əməliyyat | array | dynamic array | dynamic array + reserve |
|---|---|---|---|---|
| 1,000 | fill | 0.70 ns | 1.60 ns | 0.50 ns |
| 1,000 | insert_middle | 37.00 ns | 38.00 ns | – |
| 1,000 | erase_middle | 21.00 ns | 21.00 ns | – |
| 1,000 | linear_search | 205.50 ns | 224.50 ns | – |
| 1,000 | binary_search | 43.18 ns | 40.31 ns | – |
| 100,000 | fill | 0.78 ns | 3.77 ns | 0.61 ns |
| 100,000 | insert_middle | 4,287.00 ns | 3,776.00 ns | – |
| 100,000 | erase_middle | 3,012.00 ns | 3,194.00 ns | – |
| 100,000 | linear_search | 15,711.00 ns | 12,065.50 ns | – |
| 100,000 | binary_search | 95.07 ns | 70.81 ns | – |
| 10,000,000 | fill | 1.00 ns | 2.32 ns | 0.89 ns |
| 10,000,000 | insert_middle | 1,735,600.00 ns | 1,554,580.00 ns | – |
| 10,000,000 | erase_middle | 1,635,960.00 ns | 2,055,260.00 ns | – |
| 10,000,000 | linear_search | 1,191,980.00 ns | 1,032,760.00 ns | – |
| 10,000,000 | binary_search | 215.15 ns | 195.43 ns | – |
