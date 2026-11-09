// A2: Shallow copy problemi.
//
// ShallowIntVector, v1 IntVector-un kopyasıdır: copy constructor YOXDUR.
// Belə olanda kompilyator copy constructor-u özü yaradır və o, hər üzvü
// sadəcə kopyalayır (memberwise copy). Pointer kopyalananda isə massiv
// kopyalanmır, yalnız ÜNVAN kopyalanır. İki obyekt eyni yaddaşı göstərir.
//
// BU PROQRAM BİLƏRƏKDƏN SƏHVDİR: sonda double free baş verir.
//   Linux + ASan:  "attempting double-free" hesabatı
//   Windows:       proqram heap korrupsiyası ilə çökə bilər (və ya çökməyə də bilər:
//                  undefined behavior-un nəticəsi zəmanətli deyil)

#include <cstddef>
#include <iostream>

class ShallowIntVector {
public:
    ShallowIntVector() = default;
    ~ShallowIntVector() {
        std::cout << "    ~ShallowIntVector: delete[] " << data_ << '\n';
        delete[] data_;
    }

    void push_back(int value) {
        if (size_ == capacity_) {
            const std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;
            int* new_data = new int[new_capacity];
            for (std::size_t i = 0; i < size_; ++i) new_data[i] = data_[i];
            delete[] data_;
            data_ = new_data;
            capacity_ = new_capacity;
        }
        data_[size_++] = value;
    }

    int& operator[](std::size_t i) { return data_[i]; }
    const int* data() const { return data_; }

private:
    int* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

int main() {
    // Hər yazını dərhal ekrana çıxar. Yoxsa proqram çökəndə buferdə qalan
    // mətn itir və heç nə görmürük.
    std::cout << std::unitbuf;

    ShallowIntVector a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    {
        ShallowIntVector b = a;  // kompilyatorun yaratdığı copy constructor

        std::cout << "1) Addresses of the arrays:\n";
        std::cout << "    a.data() = " << a.data() << '\n';
        std::cout << "    b.data() = " << b.data() << "   <- SAME address!\n";

        std::cout << "\n2) b[0] = 100, then read a[0]:\n";
        b[0] = 100;
        std::cout << "    a[0] = " << a[0] << "   <- a changed too!\n";

        std::cout << "\n3) End of block: b is destroyed\n";
    }  // b-nin destructor-u ortaq massivi silir. a indi silinmiş yaddaşı göstərir.

    std::cout << "\n4) End of main: a is destroyed -> SECOND delete[] of the same address\n";
    return 0;  // a-nın destructor-u eyni ünvanı ikinci dəfə silir: DOUBLE FREE
}
