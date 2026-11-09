// B5: müqayisə operatorları, swap(a, b), mystl::erase / mystl::erase_if.

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

using test::Tracked;

namespace {

// Yalnız operator< olan köhnə üslublu tip (<=> yoxdur).
struct OnlyLess {
    int value;
    bool operator<(const OnlyLess& other) const { return value < other.value; }
    bool operator==(const OnlyLess& other) const { return value == other.value; }
};

// Heç bir sıra müqayisəsi olmayan tip.
struct Unordered {
    int value;
    bool operator==(const Unordered&) const = default;
};

template <typename V>
concept less_comparable = requires(const V& a, const V& b) { a < b; };

}  // namespace

// ---------------------------------------------------------------------------
// == və !=
// ---------------------------------------------------------------------------

TEST_CASE("equality") {
    const mystl::vector<int> a{1, 2, 3};
    const mystl::vector<int> b{1, 2, 3};
    const mystl::vector<int> shorter{1, 2};
    const mystl::vector<int> different{1, 2, 4};

    CHECK(a == b);
    CHECK_FALSE(a != b);
    CHECK(a != shorter);
    CHECK(a != different);
    CHECK(mystl::vector<int>{} == mystl::vector<int>{});
}

TEST_CASE("equality ignores capacity") {
    mystl::vector<int> a{1, 2};
    mystl::vector<int> b{1, 2};
    b.reserve(100);
    CHECK(a == b);
}

TEST_CASE("equality with strings and nested vectors") {
    const mystl::vector<std::string> a{"x", "y"};
    CHECK(a == mystl::vector<std::string>{"x", "y"});
    CHECK(a != mystl::vector<std::string>{"x", "z"});

    const mystl::vector<mystl::vector<int>> grid{{1}, {2, 3}};
    CHECK(grid == mystl::vector<mystl::vector<int>>{{1}, {2, 3}});
}

// ---------------------------------------------------------------------------
// Leksikoqrafik sıra: <, <=, >, >=, <=>
// ---------------------------------------------------------------------------

TEST_CASE("lexicographic order") {
    const mystl::vector<int> a{1, 2, 3};
    CHECK(a < mystl::vector<int>{1, 2, 4});      // ilk fərqli element: 3 < 4
    CHECK(a > mystl::vector<int>{1, 2, 2});
    CHECK(a < mystl::vector<int>{2});            // ilk element qərar verir, uzunluq yox
    CHECK(mystl::vector<int>{1, 2} < a);         // prefiks: qısa olan kiçikdir
    CHECK(mystl::vector<int>{} < mystl::vector<int>{0});
    CHECK(a <= a);
    CHECK(a >= a);
    CHECK_FALSE(a < a);
}

TEST_CASE("strings are compared like a dictionary") {
    const mystl::vector<std::string> apple{"apple", "pie"};
    const mystl::vector<std::string> banana{"banana"};
    CHECK(apple < banana);
}

TEST_CASE("operator<=> result type follows the element type") {
    const mystl::vector<int> i{1};
    const mystl::vector<double> d{1.0};
    CHECK(std::is_same_v<decltype(i <=> i), std::strong_ordering>);
    CHECK(std::is_same_v<decltype(d <=> d), std::partial_ordering>);
    CHECK((i <=> mystl::vector<int>{2}) == std::strong_ordering::less);
}

TEST_CASE("NaN makes vectors unordered") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const mystl::vector<double> a{1.0, nan};
    const mystl::vector<double> b{1.0, nan};
    CHECK((a <=> b) == std::partial_ordering::unordered);
    CHECK_FALSE(a == b);   // NaN != NaN
    CHECK_FALSE(a < b);
    CHECK_FALSE(a > b);
}

TEST_CASE("element type with only operator< (no <=>)") {
    const mystl::vector<OnlyLess> a{{1}, {2}};
    const mystl::vector<OnlyLess> b{{1}, {3}};
    CHECK(a < b);
    CHECK(std::is_same_v<decltype(a <=> b), std::weak_ordering>);
}

TEST_CASE("ordering is unavailable when the element type has no order") {
    CHECK(std::equality_comparable<mystl::vector<Unordered>>);
    CHECK_FALSE(less_comparable<mystl::vector<Unordered>>);
    CHECK(less_comparable<mystl::vector<int>>);
}

TEST_CASE("std::sort on a vector of vectors uses operator<") {
    mystl::vector<mystl::vector<int>> rows{{3, 1}, {1, 2, 3}, {1, 2}, {2}};
    std::sort(rows.begin(), rows.end());
    CHECK(rows[0] == mystl::vector<int>{1, 2});
    CHECK(rows[1] == mystl::vector<int>{1, 2, 3});
    CHECK(rows[2] == mystl::vector<int>{2});
    CHECK(rows[3] == mystl::vector<int>{3, 1});
}

// ---------------------------------------------------------------------------
// swap(a, b)
// ---------------------------------------------------------------------------

TEST_CASE("non-member swap is found by ADL and is O(1)") {
    Tracked::reset();
    mystl::vector<Tracked> a;
    a.emplace_back(1);
    mystl::vector<Tracked> b;
    b.emplace_back(2);
    b.emplace_back(3);
    const int constructed = Tracked::constructed;

    using std::swap;
    swap(a, b);                                  // ADL: mystl::swap seçilir
    CHECK(a.size() == 2);
    CHECK(b.size() == 1);
    CHECK(Tracked::constructed == constructed);  // heç bir element qurulmadı
    CHECK(noexcept(swap(a, b)));

    std::ranges::swap(a, b);
    CHECK(a.size() == 1);
}

// ---------------------------------------------------------------------------
// mystl::erase, mystl::erase_if
// ---------------------------------------------------------------------------

TEST_CASE("mystl::erase removes all equal elements and returns the count") {
    mystl::vector<int> v{0, 1, 0, 2, 0, 3};
    CHECK(mystl::erase(v, 0) == 3);
    CHECK(v == mystl::vector<int>{1, 2, 3});
    CHECK(mystl::erase(v, 42) == 0);
    CHECK(v.size() == 3);
}

TEST_CASE("mystl::erase_if removes elements matching a predicate") {
    mystl::vector<std::string> v{"keep", "", "this", "", ""};
    CHECK(mystl::erase_if(v, [](const std::string& s) { return s.empty(); }) == 3);
    CHECK(v == mystl::vector<std::string>{"keep", "this"});
}
