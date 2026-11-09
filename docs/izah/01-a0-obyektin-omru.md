# A0: Obyektin ömrü (Noisy təcrübəsi)

**Fayl:** [`experiments/a0_noisy.cpp`](../../experiments/a0_noisy.cpp)
**İşə salmaq:** `cmake --build --preset debug`, sonra `build/debug/a0_noisy.exe`

## 1. Niyə bu addımla başlayırıq?

Vektor yazmaq, əslində, **obyektlərin nə vaxt yarandığını və nə vaxt dağıdıldığını idarə etmək** deməkdir. Sonrakı bütün çətinliklər buradan gəlir: double free, sızma, `new T[]`-nin problemləri, placement new. Ona görə əvvəlcə ömrü öz gözümüzlə görürük.

## 2. Əsas anlayışlar

| Anlayış | Qısa izah |
|---|---|
| **class** | Öz məlumatı (üzv dəyişənlər) və davranışı (metodlar) olan tip |
| **constructor (ctor)** | Obyekt yarananda **avtomatik** çağırılan funksiya. Adı class-ın adıdır |
| **destructor (dtor)** | Obyekt dağıdılanda **avtomatik** çağırılan funksiya: `~Noisy()` |
| **stack** | Funksiyanın lokal dəyişənləri. Blokun `}`-ında avtomatik silinir. Sürətlidir, ölçüsü məhduddur |
| **heap** | `new` ilə alınan yaddaş. **Siz `delete` edənə qədər** yaşayır |
| `new T` / `delete p` | Heap-də 1 obyekt yaradır / dağıdıb yaddaşı qaytarır |
| `new T[n]` / `delete[] p` | Heap-də n obyektlik massiv / hamısını dağıdıb qaytarır |

## 3. Kod

```cpp
class Noisy {
public:
    Noisy() : id_(++counter_) { std::cout << "    ctor #" << id_ << '\n'; }
    ~Noisy()                  { std::cout << "    dtor #" << id_ << '\n'; }
private:
    inline static int counter_ = 0;
    int id_;
};
```

- `static`: `counter_` hər obyektə ayrıca yox, **class-a** məxsusdur. Bütün `Noisy`-lər bir sayğacı paylaşır. Ona görə hər yeni obyekt növbəti nömrəni alır.
- `inline static ... = 0`: C++17-dən etibarən static üzvü birbaşa class-ın içində init etməyə imkan verir.
- `: id_(++counter_)`: **member initializer list**. `id_` obyekt yaranarkən dəyər alır. Constructor-un gövdəsinə çatanda artıq hazırdır.

## 4. Həqiqi çıxış və izahı

```
1) Stack object inside { }
    ctor #1
    ...end of block
    dtor #1
```
`a` stack-dədir. `}`-a çatanda kompilyator destructor-u **özü** çağırır. Heç nə yazmağa ehtiyac yoxdur. **RAII** ideyası budur: resurs obyektin ömrünə bağlanır və avtomatik azad olunur.

```
2) new + delete
    ctor #2
    ...before delete
    dtor #2
```
`new Noisy` obyekti heap-də yaradır, `p` isə sadəcə onun ünvanını saxlayan stack dəyişənidir. Destructor **yalnız `delete p` sətrində** çağırılır, blokun sonunda yox.

```
3) new WITHOUT delete
    ctor #3
    ...end of block
```
**`dtor #3` heç vaxt çap olunmur.** Blok bitəndə `leak` pointer-i (stack-də idi) silindi, heap-dəki obyekt isə qaldı. Artıq onun ünvanını bilən yoxdur, ona görə onu heç kim silə bilməz. **Yaddaş sızması (memory leak)** budur.

- Proqram bitəndə əməliyyat sistemi yaddaşı geri alır. Amma uzun işləyən proqramda (server, oyun) sızmalar yığılır.
- Daha pis tərəfi: destructor çağırılmır. Obyekt fayl, şəbəkə bağlantısı və ya kilid tutursa, onlar da azad olunmur.
- Linux-da ASan (LeakSanitizer) proqramın sonunda bunu `detected memory leaks` kimi göstərəcək. Bu təcrübədə həmin xəbərdarlıq **bilərəkdəndir**.

```
4) new Noisy[3] + delete[]
    ctor #4
    ctor #5
    ctor #6
    ...before delete[]
    dtor #6
    dtor #5
    dtor #4
```
İki vacib müşahidə var:

1. **`new Noisy[3]` dərhal 3 obyekt yaradır** və hər biri üçün **default constructor** çağırır. Bunun vektor üçün böyük problem olduğunu A4-də görəcəyik:
   - Default constructor-u olmayan tip üçün `new T[n]` kompilyasiya olunmur.
   - Bizə 1000 elementlik **yer** lazımdırsa (`reserve`), `new T[1000]` 1000 lazımsız **obyekt** yaradır.
2. **Destructor-lar tərs sırayla** çağırılır: 6, 5, 4. Qayda belədir: sonra yaranan əvvəl dağıdılır, eynilə stack kimi. Səbəbi odur ki, sonra yaranan obyekt əvvəlkindən asılı ola bilər. Onu əvvəl dağıtmaq təhlükəsizdir.

## 5. Tələlər

- `new[]` ilə alınanı **`delete[]`** ilə, `new` ilə alınanı **`delete`** ilə sil. Qarışdırmaq undefined behavior-dur. Məsələn, `delete arr` yazsan, bəlkə yalnız birinci obyektin destructor-u çağırılar, bəlkə proqram çökər.
- Bir obyekti **iki dəfə** `delete` etmək (double free) də undefined behavior-dur. A2-də buna real misal görəcəyik.

## 6. Vektor üçün nə deməkdir?

Vektorun işi **yaddaşı** (capacity) və **obyektləri** (size) ayrı-ayrı idarə etməkdir. `new T[]` bu ikisini birləşdirir: yer alanda obyekt də yaradır. A4-də onları ayıracağıq: əvvəl xam yaddaş (`::operator new`), sonra lazım olan anda obyekt (placement new).

## 7. Müdafiədə gələ biləcək suallar

**Stack və heap-in fərqi nədir?** Stack-dəki obyekt blok bitəndə avtomatik dağıdılır, ölçüsü kompilyasiya vaxtı məlum olmalıdır. Heap-dəki obyekt `delete` edilənə qədər yaşayır, ölçüsü işləmə vaxtı seçilə bilər. Vektor elementləri heap-də saxlamalıdır, çünki neçə element olacağı əvvəlcədən bilinmir.

**Destructor-lar niyə tərs sırada çağırılır?** Sonra yaradılan obyekt əvvəlkilərdən asılı ola bilər. Ona görə sonuncu birinci dağıdılır.

**`delete`-siz `new` nəyə gətirib çıxarır?** Yaddaş sızmasına. Destructor heç vaxt çağırılmır, obyektin tutduğu resurslar azad olunmur.
