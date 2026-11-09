#pragma once

// Testlər üçün köməkçi tiplər.

#include <stdexcept>

namespace test {

// Sayğac tipi: neçə obyektin qurulduğunu, dağıdıldığını, kopyalandığını və
// köçürüldüyünü sayır.
//
// Windows-da ASan olmadığı üçün yaddaş sızmasını və ikiqat destructor-u
// bu sayğaclarla tuturuq: test bitəndə live() == 0 olmalıdır.
//
// Default constructor-u YOXDUR: vector-un ondan asılı olmadığını da yoxlayır.
//
// Copy constructor istənilən anda exception atmağa proqramlaşdırıla bilər:
// throw_on_copy_after = k  =>  növbəti k kopya uğurlu olur, (k+1)-ci exception atır.
//
// NoexceptMove parametri move constructor-un noexcept olub-olmadığını seçir:
//   Tracked              (true):  vector böyüyəndə elementləri KÖÇÜRÜR
//   TrackedThrowingMove  (false): vector böyüyəndə elementləri KOPYALAYIR
//                                 (std::move_if_noexcept belə qərar verir)
// Hər variantın sayğacları ayrıdır (static üzvlər hər template instansiyasında ayrıdır).
template <bool NoexceptMove>
struct BasicTracked {
    inline static int constructed = 0;          // bütün constructor çağırışları
    inline static int destroyed = 0;            // destructor çağırışları
    inline static int copies = 0;               // uğurlu copy constructor çağırışları
    inline static int moves = 0;                // move constructor çağırışları
    inline static int throw_on_copy_after = -1; // -1: heç vaxt atma

    static void reset() {
        constructed = 0;
        destroyed = 0;
        copies = 0;
        moves = 0;
        throw_on_copy_after = -1;
    }
    static int live() { return constructed - destroyed; }

    explicit BasicTracked(int v) : value(v) { ++constructed; }

    BasicTracked(const BasicTracked& other) : value(other.value) {
        if (throw_on_copy_after == 0) {
            throw std::runtime_error("Tracked: copy failed");
        }
        if (throw_on_copy_after > 0) {
            --throw_on_copy_after;
        }
        ++constructed;
        ++copies;
    }

    // Köçürülən obyekt (moved-from) -1 dəyəri ilə işarələnir.
    BasicTracked(BasicTracked&& other) noexcept(NoexceptMove) : value(other.value) {
        other.value = -1;
        ++constructed;
        ++moves;
    }

    BasicTracked& operator=(const BasicTracked& other) {
        value = other.value;
        return *this;
    }

    BasicTracked& operator=(BasicTracked&& other) noexcept(NoexceptMove) {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~BasicTracked() { ++destroyed; }

    int value;
};

using Tracked = BasicTracked<true>;
using TrackedThrowingMove = BasicTracked<false>;

}  // namespace test
