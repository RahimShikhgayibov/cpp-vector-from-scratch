#include "doctest.h"

#include <mystl/vector.hpp>

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------------
// vector<int>: v1/v2-dən gələn əsas məntiq
// ---------------------------------------------------------------------------

TEST_CASE("vector<int>: new vector is empty") {
    mystl::vector<int> v;
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 0);
}

TEST_CASE("vector<int>: push_back stores values in order") {
    mystl::vector<int> v;
    v.push_back(10);
    v.push_back(20);
    v.push_back(30);

    REQUIRE(v.size() == 3);
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);
}

TEST_CASE("vector<int>: operator[] can modify an element") {
    mystl::vector<int> v;
    v.push_back(1);
    v[0] = 42;
    CHECK(v[0] == 42);
}

TEST_CASE("vector<int>: const object can be read") {
    mystl::vector<int> v;
    v.push_back(7);
    const mystl::vector<int>& cv = v;
    CHECK(cv.size() == 1);
    CHECK(cv[0] == 7);
}

TEST_CASE("vector<int>: capacity doubles 1, 2, 4, 8, ...") {
    mystl::vector<int> v;
    std::vector<std::size_t> seen;
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
        if (seen.empty() || seen.back() != v.capacity()) {
            seen.push_back(v.capacity());
        }
    }
    const std::vector<std::size_t> expected{1, 2, 4, 8, 16, 32, 64, 128};
    CHECK(seen == expected);
}

TEST_CASE("vector<int>: size never exceeds capacity") {
    mystl::vector<int> v;
    for (int i = 0; i < 1000; ++i) {
        v.push_back(i);
        CHECK(v.size() <= v.capacity());
    }
}

TEST_CASE("vector<int>: 1 000 000 push_back are all correct") {
    constexpr int n = 1'000'000;
    mystl::vector<int> v;
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
    }
    REQUIRE(v.size() == static_cast<std::size_t>(n));

    std::size_t first_wrong = v.size();
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v[i] != static_cast<int>(i)) {
            first_wrong = i;
            break;
        }
    }
    CHECK(first_wrong == v.size());
    CHECK(v.capacity() == 1u << 20);
}

// ---------------------------------------------------------------------------
// Tip adları
// ---------------------------------------------------------------------------

TEST_CASE("vector<T>: member type aliases") {
    using V = mystl::vector<std::string>;
    CHECK(std::is_same_v<V::value_type, std::string>);
    CHECK(std::is_same_v<V::size_type, std::size_t>);
    CHECK(std::is_same_v<V::reference, std::string&>);
    CHECK(std::is_same_v<V::const_reference, const std::string&>);
}

// ---------------------------------------------------------------------------
// vector<std::string>: heap-ə sahib olan element tipi
// ---------------------------------------------------------------------------

namespace {

// Small String Optimization-dan (SSO) qaçmaq üçün uzun sətir.
// Qısa sətirlər std::string-in içində saxlanır və heap istifadə etmir.
// Uzun sətir isə heap-də olur: kopyalama səhvlərini yalnız belə sətirlər göstərir.
std::string long_string(int i) {
    return "a fairly long string that does not fit in SSO #" + std::to_string(i);
}

}  // namespace

TEST_CASE("vector<std::string>: push_back and read back") {
    mystl::vector<std::string> v;
    v.push_back("short");
    v.push_back(long_string(1));

    REQUIRE(v.size() == 2);
    CHECK(v[0] == "short");
    CHECK(v[1] == long_string(1));
}

TEST_CASE("vector<std::string>: strings survive many grow() calls") {
    mystl::vector<std::string> v;
    for (int i = 0; i < 1000; ++i) {
        v.push_back(long_string(i));
    }
    REQUIRE(v.size() == 1000);
    for (int i = 0; i < 1000; ++i) {
        CHECK(v[static_cast<std::size_t>(i)] == long_string(i));
    }
}

TEST_CASE("vector<std::string>: modifying an element through operator[]") {
    mystl::vector<std::string> v;
    v.push_back("abc");
    v[0] += "def";  // reference qaytarıldığı üçün yerində dəyişir
    CHECK(v[0] == "abcdef");
}

TEST_CASE("vector<std::string>: push_back does not alias the argument") {
    mystl::vector<std::string> v;
    std::string s = long_string(7);
    v.push_back(s);
    s = "changed";              // orijinalı dəyiş
    CHECK(v[0] == long_string(7));  // vektordakı kopya dəyişməyib
}

// ---------------------------------------------------------------------------
// vector<vector<int>>: element tipinin özü də bizim vector-dur
// ---------------------------------------------------------------------------

TEST_CASE("vector<vector<int>>: jagged 2D structure") {
    mystl::vector<mystl::vector<int>> grid;
    for (int row = 0; row < 10; ++row) {
        mystl::vector<int> r;
        for (int col = 0; col <= row; ++col) {  // hər sətir fərqli uzunluqda
            r.push_back(row * 100 + col);
        }
        grid.push_back(r);
    }

    REQUIRE(grid.size() == 10);
    for (std::size_t row = 0; row < grid.size(); ++row) {
        REQUIRE(grid[row].size() == row + 1);
        for (std::size_t col = 0; col < grid[row].size(); ++col) {
            CHECK(grid[row][col] == static_cast<int>(row * 100 + col));
        }
    }
}

TEST_CASE("vector<vector<int>>: inner vectors can grow in place") {
    mystl::vector<mystl::vector<int>> grid;
    grid.push_back(mystl::vector<int>{});
    grid.push_back(mystl::vector<int>{});
    grid[1].push_back(5);
    grid[1].push_back(6);

    CHECK(grid[0].size() == 0);
    REQUIRE(grid[1].size() == 2);
    CHECK(grid[1][1] == 6);
}

// ---------------------------------------------------------------------------
// Digər tiplər
// ---------------------------------------------------------------------------

namespace {
struct Point {
    double x = 0;
    double y = 0;
};
}  // namespace

TEST_CASE("vector<T>: works with double and a user-defined struct") {
    mystl::vector<double> d;
    d.push_back(1.5);
    d.push_back(-2.25);
    CHECK(d[1] == doctest::Approx(-2.25));

    mystl::vector<Point> p;
    p.push_back(Point{1, 2});
    p.push_back(Point{3, 4});
    CHECK(p[1].x == doctest::Approx(3));
    CHECK(p[1].y == doctest::Approx(4));
}
