// A0: Obyektin ömrü.
// Noisy hər constructor və destructor çağırışında ekrana yazır.
// Belə çıxış görürük: obyekt NƏ VAXT yaranır, NƏ VAXT dağıdılır.

#include <iostream>

class Noisy {
public:
    Noisy() : id_(++counter_) {
        std::cout << "    ctor #" << id_ << '\n';
    }

    ~Noisy() {
        std::cout << "    dtor #" << id_ << '\n';
    }

private:
    inline static int counter_ = 0;  // bütün Noisy obyektləri üçün ortaq sayğac
    int id_;
};

int main() {
    std::cout << "1) Stack object inside { }\n";
    {
        Noisy a;
        std::cout << "    ...end of block\n";
    }  // <- a burada avtomatik dağıdılır

    std::cout << "\n2) new + delete\n";
    {
        Noisy* p = new Noisy;  // obyekt heap-də, p isə stack-dədir
        std::cout << "    ...before delete\n";
        delete p;              // dtor yalnız burada çağırılır
    }

    std::cout << "\n3) new WITHOUT delete\n";
    {
        Noisy* leak = new Noisy;
        (void)leak;            // "istifadə olunmayan dəyişən" warning-ini susdurur
        std::cout << "    ...end of block\n";
    }  // <- pointer itdi, obyekt isə heap-də qaldı: dtor HEÇ VAXT çağırılmır

    std::cout << "\n4) new Noisy[3] + delete[]\n";
    {
        Noisy* arr = new Noisy[3];  // 3 obyekt: ctor #4, #5, #6
        std::cout << "    ...before delete[]\n";
        delete[] arr;               // dtor tərs sırayla: #6, #5, #4
    }

    std::cout << "\nend of main\n";
}
