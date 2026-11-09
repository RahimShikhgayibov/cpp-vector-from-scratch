#include "doctest.h"

#include <mystl/int_vector.hpp>

#include <cstddef>
#include <vector>

TEST_CASE("IntVector: new vector is empty") {
    mystl::IntVector v;
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 0);
}

TEST_CASE("IntVector: push_back stores values in order") {
    mystl::IntVector v;
    v.push_back(10);
    v.push_back(20);
    v.push_back(30);

    REQUIRE(v.size() == 3);
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);
}

TEST_CASE("IntVector: operator[] can modify an element") {
    mystl::IntVector v;
    v.push_back(1);
    v[0] = 42;
    CHECK(v[0] == 42);
}

TEST_CASE("IntVector: const object can be read") {
    mystl::IntVector v;
    v.push_back(7);
    const mystl::IntVector& cv = v;  // const metodlar çağırılmalıdır
    CHECK(cv.size() == 1);
    CHECK(cv[0] == 7);
}

TEST_CASE("IntVector: capacity doubles 1, 2, 4, 8, ...") {
    mystl::IntVector v;
    std::vector<std::size_t> seen;  // capacity-nin aldığı hər fərqli dəyər
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
        if (seen.empty() || seen.back() != v.capacity()) {
            seen.push_back(v.capacity());
        }
    }
    const std::vector<std::size_t> expected{1, 2, 4, 8, 16, 32, 64, 128};
    CHECK(seen == expected);
}

TEST_CASE("IntVector: size never exceeds capacity") {
    mystl::IntVector v;
    for (int i = 0; i < 1000; ++i) {
        v.push_back(i);
        CHECK(v.size() <= v.capacity());
    }
}

TEST_CASE("IntVector: 1 000 000 push_back are all correct") {
    constexpr int n = 1'000'000;
    mystl::IntVector v;
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
    }
    REQUIRE(v.size() == static_cast<std::size_t>(n));

    // Hər elementi CHECK etmək 1 000 000 sətirlik hesabat verərdi.
    // Ona görə əvvəl səhv olanı axtarırıq, sonra bir dəfə yoxlayırıq.
    std::size_t first_wrong = v.size();
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v[i] != static_cast<int>(i)) {
            first_wrong = i;
            break;
        }
    }
    CHECK(first_wrong == v.size());
    CHECK(v.capacity() == 1u << 20);  // 2^20 = 1 048 576 >= 1 000 000
}
