# B5: Müqayisə operatorları və ümumi funksiyalar

**Fayllar:** [`include/mystl/vector.hpp`](../../include/mystl/vector.hpp), [`tests/test_vector_compare.cpp`](../../tests/test_vector_compare.cpp)

## 1. Əlavə olunanlar

| Funksiya | Nə edir | Mürəkkəblik |
|---|---|---|
| `a == b` (və avtomatik `!=`) | eyni ölçü və bütün elementlər bərabərdir | O(n) |
| `a <=> b` (və avtomatik `<`, `<=`, `>`, `>=`) | leksikoqrafik (lüğət) sırası | O(n) |
| `swap(a, b)` | iki vektorun məzmununu dəyişir | O(1) |
| `mystl::erase(v, value)` | bütün `value`-ları silir, sayını qaytarır | O(n) |
| `mystl::erase_if(v, pred)` | şərtə uyğun olanları silir, sayını qaytarır | O(n) |

## 2. Bərabərlik

```cpp
friend bool operator==(const vector& a, const vector& b) {
    return a.size_ == b.size_ && std::equal(a.begin(), a.end(), b.begin());
}
```

- Əvvəl **ölçü** müqayisə olunur: fərqlidirsə, cavab O(1)-də `false` olur və elementlərə baxılmır.
- `capacity` müqayisəyə **daxil deyil**: bərabərlik məzmuna aiddir, yaddaşa yox. Test bunu yoxlayır.
- **`!=` yazmamışıq.** C++20-də kompilyator `a != b` ifadəsini avtomatik `!(a == b)` kimi yazır (**rewritten candidate**).

## 3. Leksikoqrafik sıra və `operator<=>` (C++20)

Lüğətdəki sıra kimidir:
1. Elementlər başdan bir-bir müqayisə olunur; **ilk fərqli element** qərar verir: `{1, 2, 3} < {1, 2, 4}`.
2. Fərq yoxdursa, amma biri digərinin **başlanğıcıdırsa**, qısa olan kiçikdir: `{1, 2} < {1, 2, 3}`.
3. Uzunluq ilk növbədə **qərar vermir**: `{1, 2, 3} < {2}`, çünki `1 < 2`.

### "Spaceship" operatoru

```cpp
friend auto operator<=>(const vector& a, const vector& b)
    requires detail::synth_comparable<T>
{
    return std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end(),
                                                  detail::synth_three_way);
}
```

`a <=> b` bir dəfəyə üç cavabdan birini verir: kiçik, bərabər və ya böyük. C++20-də kompilyator `a < b` ifadəsini `(a <=> b) < 0` kimi yazır, `<=`, `>`, `>=` üçün də eyni qayda işləyir. Beləliklə **bir operator dördünü əvəz edir**. C++17-də isə 6 ayrı operator yazmaq lazım idi.

### Nəticənin tipi `T`-dən asılıdır

| `T` | `vector<T> <=> vector<T>` tipi | Mənası |
|---|---|---|
| `int`, `std::string` | `std::strong_ordering` | bərabər olanlar tam eynidir |
| `double` | `std::partial_ordering` | bəzi dəyərlər müqayisə olunmur (**NaN**) |
| yalnız `<` olan tip | `std::weak_ordering` | ekvivalent olanlar fərqli ola bilər |

`auto` qaytarma tipi elementlərin tipindən avtomatik çıxarılır. Test: `vector<double>{1.0, NaN}`-ı özü ilə müqayisə edəndə `unordered` alınır, `==` də `false` verir, çünki `NaN != NaN`.

### Yalnız `operator<` olan tiplər: `synth_three_way`

C++20-dən əvvəl yazılmış çox tipdə `<=>` yoxdur, yalnız `<` var. `std::vector` onları da müqayisə edə bilir. Biz də eyni üsulla edirik:

```cpp
if constexpr (std::three_way_comparable<T>) {
    return a <=> b;                                  // varsa, birbaşa
} else {
    if (a < b) return std::weak_ordering::less;      // yoxdursa, < ilə qur
    if (b < a) return std::weak_ordering::greater;
    return std::weak_ordering::equivalent;
}
```

Standart bu üsulu *synth-three-way* adlandırır. Test: yalnız `<` olan `OnlyLess` tipi ilə `a < b` işləyir və nəticə `weak_ordering` olur.

### `requires`: sıra olmayan tiplər

Heç `<`-i də olmayan tip üçün (`Unordered`) `operator<=>` **mövcud olmamalıdır**. `requires detail::synth_comparable<T>` bunu təmin edir. Sıra olmayan tip üçün `a < b` kompilyasiya olunmur, aydın xəta verir. Test konseptlə yoxlayır: `vector<Unordered>` bərabərlik üçün müqayisə oluna bilir, sıra üçün isə yox.

## 4. Hidden friend

Operatorlar və `swap` **sinfin içində `friend`** kimi təyin olunub:

```cpp
friend bool operator==(const vector& a, const vector& b) { ... }
```

Bunlara **hidden friend** deyilir. Belə funksiyalar adi axtarışda görünmür və yalnız **ADL** (argument-dependent lookup) ilə tapılır, yəni arqumentlərdən biri `mystl::vector` olanda. Faydaları:
- kompilyator başqa tiplərin müqayisəsində bu operatorları nəzərə almır, xəta mesajları qısa olur, kompilyasiya sürətlənir;
- gizli çevirmələr baş vermir;
- `friend` olduğu üçün `size_` kimi private üzvləri görür.

## 5. `swap(a, b)`

```cpp
friend void swap(vector& a, vector& b) noexcept { a.swap(b); }
```

Ümumi (generic) kodda swap belə çağırılır:

```cpp
using std::swap;
swap(a, b);
```

ADL əvvəlcə tipin öz namespace-ində (`mystl`) axtarır və bizim versiyanı tapır. Tip öz `swap`-ını təqdim etmirsə, `std::swap` istifadə olunur. `std::sort` kimi alqoritmlər də elementləri bu üsulla dəyişir.

`std::swap(a, b)` də işləyərdi (move constructor + 2 move assignment), amma bizim versiya birbaşa 3 üzvü dəyişir. Test: heç bir element qurulmur və `noexcept`-dir.

## 6. `mystl::erase` və `mystl::erase_if` (C++20)

B4-dəki erase–remove idiomunu bir çağırışa yığır:

```cpp
mystl::erase(v, 0);                                   // əvəzinə: v.erase(std::remove(...), v.end());
mystl::erase_if(v, [](int x) { return x < 0; });
```

Silinən elementlərin sayını qaytarır. Bu, C++20-dəki `std::erase(std::vector&, value)`-un analoqudur. Üzv funksiyası yox, **sərbəst funksiyadır**: standart da bunu bütün konteynerlər üçün eyni formada təqdim edir.

## 7. Testlər

| Qrup | Nəyi yoxlayır |
|---|---|
| bərabərlik | eyni, qısa, fərqli element, boş; `capacity` nəzərə alınmır; `string` və iç-içə vektorlar |
| sıra | ilk fərqli element, prefiks, boş vektor, `<=`/`>=`; sətirlər lüğət sırası ilə |
| nəticə tipi | `int` → `strong_ordering`, `double` → `partial_ordering` |
| NaN | `unordered`, `==` false |
| yalnız `<` | `weak_ordering` ilə işləyir |
| sırasız tip | `<` mövcud deyil (konsept), `==` var |
| `std::sort` | `vector<vector<int>>` sıralanır |
| `swap` | ADL, O(1), `noexcept`, `std::ranges::swap` |
| `erase`, `erase_if` | silinənlərin sayı, nəticə |

**Nəticə:** 148 test, 7791 yoxlama, hamısı keçir; build-də warning yoxdur.

## 8. Müdafiədə gələ biləcək suallar

**İki vektor necə müqayisə olunur?**
Bərabərlik: eyni ölçü və bütün elementlər bərabər. Sıra: leksikoqrafik, ilk fərqli element qərar verir, biri digərinin prefiksidirsə qısa olan kiçikdir.

**`operator<=>` nədir, nə üstünlüyü var?**
C++20-nin üç tərəfli müqayisə operatorudur. Bir dəfə yazılır, kompilyator `<`, `<=`, `>`, `>=` ifadələrini ondan özü qurur. Nəticə tipi (`strong`/`weak`/`partial_ordering`) müqayisənin xüsusiyyətini də bildirir.

**`vector<double>` müqayisəsində nə xüsusidir?**
NaN heç nə ilə, hətta özü ilə də müqayisə olunmur. Ona görə nəticə `partial_ordering`-dir və NaN olan vektorlar `unordered` ola bilər.

**Hidden friend nədir?**
Sinfin içində təyin olunmuş `friend` funksiyadır. Yalnız ADL ilə tapılır: lazımsız overload-lar və gizli çevirmələr olmur, private üzvlərə də çıxış var.

**Niyə öz `swap`-ımız var, `std::swap` kifayət deyilmi?**
`std::swap` move constructor və iki move assignment edir. Bizimki birbaşa üç üzvü dəyişir: daha sadədir və `noexcept` olduğu açıq-aydın görünür. ADL ilə generic kod onu avtomatik tapır.
