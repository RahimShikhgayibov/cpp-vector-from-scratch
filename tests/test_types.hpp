#pragma once

// Testlər üçün köməkçi tiplər.

#include <stdexcept>

namespace test {

// Sayğac tipi: neçə obyektin qurulduğunu və dağıdıldığını sayır.
//
// Windows-da ASan olmadığı üçün yaddaş sızmasını və ikiqat destructor-u
// bu sayğaclarla tuturuq: test bitəndə live() == 0 olmalıdır.
//
// Default constructor-u YOXDUR: vector-un ondan asılı olmadığını da yoxlayır.
//
// Copy constructor istənilən anda exception atmağa proqramlaşdırıla bilər:
// throw_on_copy_after = k  =>  növbəti k kopya uğurlu olur, (k+1)-ci exception atır.
struct Tracked {
    inline static int constructed = 0;          // bütün constructor çağırışları
    inline static int destroyed = 0;            // destructor çağırışları
    inline static int copies = 0;               // uğurlu copy constructor çağırışları
    inline static int throw_on_copy_after = -1; // -1: heç vaxt atma

    static void reset() {
        constructed = 0;
        destroyed = 0;
        copies = 0;
        throw_on_copy_after = -1;
    }
    static int live() { return constructed - destroyed; }

    explicit Tracked(int v) : value(v) { ++constructed; }

    Tracked(const Tracked& other) : value(other.value) {
        if (throw_on_copy_after == 0) {
            throw std::runtime_error("Tracked: copy failed");
        }
        if (throw_on_copy_after > 0) {
            --throw_on_copy_after;
        }
        ++constructed;
        ++copies;
    }

    Tracked& operator=(const Tracked& other) {
        value = other.value;
        return *this;
    }

    ~Tracked() { ++destroyed; }

    int value;
};

}  // namespace test
