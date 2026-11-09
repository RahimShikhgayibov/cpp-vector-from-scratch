#include "doctest.h"

#include <mystl/int_vector.hpp>

#include <cstddef>

namespace {

// 0, 1, 2, ..., n-1 ilə doldurulmuş vektor
mystl::IntVector make_vector(int n) {
    mystl::IntVector v;
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
    }
    return v;
}

}  // namespace

TEST_CASE("IntVector copy ctor: copy has the same elements") {
    const mystl::IntVector a = make_vector(5);
    const mystl::IntVector b = a;  // copy constructor

    REQUIRE(b.size() == a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(b[i] == a[i]);
    }
}

TEST_CASE("IntVector copy ctor: copy is independent (deep copy)") {
    mystl::IntVector a = make_vector(3);
    mystl::IntVector b = a;

    b[0] = 100;          // kopyanı dəyiş
    b.push_back(42);     // kopyanı böyüt

    CHECK(a[0] == 0);    // orijinal dəyişməyib
    CHECK(a.size() == 3);
    CHECK(b[0] == 100);
    CHECK(b.size() == 4);

    a[1] = -1;           // və əksinə
    CHECK(b[1] == 1);
}

TEST_CASE("IntVector copy ctor: copy of an empty vector") {
    const mystl::IntVector a;
    mystl::IntVector b = a;
    CHECK(b.size() == 0);
    CHECK(b.capacity() == 0);

    b.push_back(1);      // boş kopya istifadəyə yararlıdır
    CHECK(b[0] == 1);
    CHECK(a.size() == 0);
}

TEST_CASE("IntVector copy ctor: copy does not keep spare capacity") {
    mystl::IntVector a = make_vector(5);  // capacity 8
    REQUIRE(a.capacity() == 8);
    const mystl::IntVector b = a;
    CHECK(b.capacity() == 5);
}

TEST_CASE("IntVector copy assignment: into a smaller and a larger vector") {
    const mystl::IntVector big = make_vector(10);
    const mystl::IntVector small = make_vector(2);

    SUBCASE("small target gets bigger") {
        mystl::IntVector t = make_vector(1);
        t = big;
        REQUIRE(t.size() == 10);
        CHECK(t[9] == 9);
    }
    SUBCASE("big target gets smaller") {
        mystl::IntVector t = make_vector(20);
        t = small;
        REQUIRE(t.size() == 2);
        CHECK(t[0] == 0);
        CHECK(t[1] == 1);
    }
    SUBCASE("empty source empties the target") {
        mystl::IntVector t = make_vector(20);
        t = mystl::IntVector{};
        CHECK(t.size() == 0);
    }
}

TEST_CASE("IntVector copy assignment: result is independent") {
    mystl::IntVector a = make_vector(3);
    mystl::IntVector b;
    b = a;
    b[2] = 99;
    CHECK(a[2] == 2);
}

TEST_CASE("IntVector copy assignment: self-assignment is safe") {
    mystl::IntVector a = make_vector(4);
    // `a = a;` yazsaq, bəzi kompilyatorlar warning verir. Real proqramlarda
    // self-assignment adətən alias vasitəsilə olur, məsələn: v[i] = v[j].
    mystl::IntVector& alias = a;
    a = alias;

    REQUIRE(a.size() == 4);
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i] == static_cast<int>(i));
    }
}

TEST_CASE("IntVector copy assignment: chaining a = b = c") {
    const mystl::IntVector c = make_vector(3);
    mystl::IntVector a;
    mystl::IntVector b;
    a = b = c;  // operator= reference qaytardığı üçün işləyir
    CHECK(a.size() == 3);
    CHECK(b.size() == 3);
    CHECK(a[2] == 2);
}

TEST_CASE("IntVector swap: exchanges contents") {
    mystl::IntVector a = make_vector(2);
    mystl::IntVector b = make_vector(5);
    a.swap(b);
    CHECK(a.size() == 5);
    CHECK(b.size() == 2);
    CHECK(a[4] == 4);
    CHECK(b[1] == 1);
}
