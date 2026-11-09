#include "doctest.h"

#include <mystl/vector.hpp>

#include <cstddef>
#include <string>

namespace {

// 0, 1, 2, ..., n-1 ilə doldurulmuş vektor
mystl::vector<int> make_vector(int n) {
    mystl::vector<int> v;
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
    }
    return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// vector<int>
// ---------------------------------------------------------------------------

TEST_CASE("copy ctor: copy has the same elements") {
    const mystl::vector<int> a = make_vector(5);
    const mystl::vector<int> b = a;

    REQUIRE(b.size() == a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(b[i] == a[i]);
    }
}

TEST_CASE("copy ctor: copy is independent (deep copy)") {
    mystl::vector<int> a = make_vector(3);
    mystl::vector<int> b = a;

    b[0] = 100;
    b.push_back(42);

    CHECK(a[0] == 0);
    CHECK(a.size() == 3);
    CHECK(b[0] == 100);
    CHECK(b.size() == 4);

    a[1] = -1;
    CHECK(b[1] == 1);
}

TEST_CASE("copy ctor: copy of an empty vector") {
    const mystl::vector<int> a;
    mystl::vector<int> b = a;
    CHECK(b.size() == 0);
    CHECK(b.capacity() == 0);

    b.push_back(1);
    CHECK(b[0] == 1);
    CHECK(a.size() == 0);
}

TEST_CASE("copy ctor: copy does not keep spare capacity") {
    mystl::vector<int> a = make_vector(5);
    REQUIRE(a.capacity() == 8);
    const mystl::vector<int> b = a;
    CHECK(b.capacity() == 5);
}

TEST_CASE("copy assignment: into a smaller and a larger vector") {
    const mystl::vector<int> big = make_vector(10);
    const mystl::vector<int> small = make_vector(2);

    SUBCASE("small target gets bigger") {
        mystl::vector<int> t = make_vector(1);
        t = big;
        REQUIRE(t.size() == 10);
        CHECK(t[9] == 9);
    }
    SUBCASE("big target gets smaller") {
        mystl::vector<int> t = make_vector(20);
        t = small;
        REQUIRE(t.size() == 2);
        CHECK(t[0] == 0);
        CHECK(t[1] == 1);
    }
    SUBCASE("empty source empties the target") {
        mystl::vector<int> t = make_vector(20);
        t = mystl::vector<int>{};
        CHECK(t.size() == 0);
    }
}

TEST_CASE("copy assignment: result is independent") {
    mystl::vector<int> a = make_vector(3);
    mystl::vector<int> b;
    b = a;
    b[2] = 99;
    CHECK(a[2] == 2);
}

TEST_CASE("copy assignment: self-assignment is safe") {
    mystl::vector<int> a = make_vector(4);
    mystl::vector<int>& alias = a;
    a = alias;

    REQUIRE(a.size() == 4);
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i] == static_cast<int>(i));
    }
}

TEST_CASE("copy assignment: chaining a = b = c") {
    const mystl::vector<int> c = make_vector(3);
    mystl::vector<int> a;
    mystl::vector<int> b;
    a = b = c;
    CHECK(a.size() == 3);
    CHECK(b.size() == 3);
    CHECK(a[2] == 2);
}

TEST_CASE("swap: exchanges contents") {
    mystl::vector<int> a = make_vector(2);
    mystl::vector<int> b = make_vector(5);
    a.swap(b);
    CHECK(a.size() == 5);
    CHECK(b.size() == 2);
    CHECK(a[4] == 4);
    CHECK(b[1] == 1);
}

// ---------------------------------------------------------------------------
// vector<std::string> və vector<vector<int>>: elementlər özləri də dərin kopyalanmalıdır
// ---------------------------------------------------------------------------

TEST_CASE("copy ctor: vector<std::string> copies are independent") {
    mystl::vector<std::string> a;
    a.push_back("first string that is long enough to live on the heap");
    a.push_back("second");

    mystl::vector<std::string> b = a;
    b[0] += " (changed)";

    CHECK(a[0] == "first string that is long enough to live on the heap");
    CHECK(b[0] == "first string that is long enough to live on the heap (changed)");
}

TEST_CASE("copy ctor: vector<vector<int>> copies inner vectors deeply") {
    mystl::vector<mystl::vector<int>> a;
    a.push_back(make_vector(3));
    a.push_back(make_vector(5));

    mystl::vector<mystl::vector<int>> b = a;  // xarici kopya daxili vektorları da kopyalayır
    b[0][0] = 100;                            // daxili vektoru dəyiş
    b[1].push_back(42);                       // daxili vektoru böyüt

    CHECK(a[0][0] == 0);       // orijinalın daxili vektoru dəyişməyib
    CHECK(a[1].size() == 5);
    CHECK(b[0][0] == 100);
    CHECK(b[1].size() == 6);
}

TEST_CASE("copy assignment: vector<vector<int>> self-assignment is safe") {
    mystl::vector<mystl::vector<int>> a;
    a.push_back(make_vector(2));
    mystl::vector<mystl::vector<int>>& alias = a;
    a = alias;
    REQUIRE(a.size() == 1);
    REQUIRE(a[0].size() == 2);
    CHECK(a[0][1] == 1);
}
