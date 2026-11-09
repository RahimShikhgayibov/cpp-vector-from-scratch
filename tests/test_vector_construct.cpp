// B2: constructor-lar və mənimsəmə.

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <array>
#include <cstddef>
#include <forward_list>
#include <iterator>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using test::Tracked;

namespace {

// Default constructor-u çağırılanda sayan tip.
struct DefaultCounted {
    inline static int defaults = 0;
    DefaultCounted() { ++defaults; }
    int value = 7;
};

}  // namespace

// ---------------------------------------------------------------------------
// vector(count)
// ---------------------------------------------------------------------------

TEST_CASE("vector(count): count value-initialized elements") {
    mystl::vector<int> v(5);
    REQUIRE(v.size() == 5);
    CHECK(v.capacity() == 5);
    for (int x : v) {
        CHECK(x == 0);            // value-initialization: int üçün 0, "zibil" yox
    }

    mystl::vector<std::string> s(3);
    REQUIRE(s.size() == 3);
    CHECK(s[2].empty());
}

TEST_CASE("vector(count): default constructor called exactly count times") {
    DefaultCounted::defaults = 0;
    mystl::vector<DefaultCounted> v(4);
    CHECK(DefaultCounted::defaults == 4);
    CHECK(v[3].value == 7);
}

TEST_CASE("vector(0) is empty and allocates nothing") {
    mystl::vector<int> v(0);
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 0);
    CHECK(v.data() == nullptr);
}

TEST_CASE("vector(count) is explicit") {
    CHECK_FALSE(std::is_convertible_v<std::size_t, mystl::vector<int>>);
    // mystl::vector<int> v = 5;   // kompilyasiya olunmaz
}

// ---------------------------------------------------------------------------
// vector(count, value)
// ---------------------------------------------------------------------------

TEST_CASE("vector(count, value): count copies") {
    const std::string word = "a string long enough to live on the heap";
    mystl::vector<std::string> v(3, word);
    REQUIRE(v.size() == 3);
    for (const auto& s : v) {
        CHECK(s == word);
    }
}

TEST_CASE("vector<int>(5, 1) vs vector<int>{5, 1}") {
    mystl::vector<int> parens(5, 1);   // 5 ədəd 1
    mystl::vector<int> braces{5, 1};   // iki element: 5 və 1 (initializer_list)

    CHECK(parens.size() == 5);
    CHECK(parens[4] == 1);
    REQUIRE(braces.size() == 2);
    CHECK(braces[0] == 5);
    CHECK(braces[1] == 1);
}

TEST_CASE("vector(count, value): exception while copying leaks nothing") {
    Tracked::reset();
    {
        const Tracked prototype(1);
        Tracked::throw_on_copy_after = 3;          // 4-cü kopya atır
        CHECK_THROWS_AS(mystl::vector<Tracked>(10, prototype), std::runtime_error);
        Tracked::throw_on_copy_after = -1;
        CHECK(Tracked::live() == 1);               // yalnız prototype
    }
    CHECK(Tracked::live() == 0);
}

// ---------------------------------------------------------------------------
// vector(first, last)
// ---------------------------------------------------------------------------

TEST_CASE("range ctor: from a C array and std::array") {
    const int raw[] = {3, 1, 4, 1, 5};
    mystl::vector<int> a(std::begin(raw), std::end(raw));
    REQUIRE(a.size() == 5);
    CHECK(a[2] == 4);
    CHECK(a.capacity() == 5);   // forward iterator: dəqiq yer bir dəfə ayrıldı

    const std::array<int, 3> arr{7, 8, 9};
    mystl::vector<int> b(arr.begin(), arr.end());
    CHECK(b.back() == 9);
}

TEST_CASE("range ctor: from std::list and std::forward_list") {
    const std::list<std::string> list{"x", "y", "z"};
    mystl::vector<std::string> v(list.begin(), list.end());
    REQUIRE(v.size() == 3);
    CHECK(v[1] == "y");

    const std::forward_list<int> fl{1, 2, 3, 4};
    mystl::vector<int> w(fl.begin(), fl.end());
    CHECK(w.size() == 4);
    CHECK(w.capacity() == 4);
}

TEST_CASE("range ctor: from a single-pass input iterator") {
    std::istringstream input("10 20 30 40");
    mystl::vector<int> v{std::istream_iterator<int>(input), std::istream_iterator<int>()};
    REQUIRE(v.size() == 4);
    CHECK(v[0] == 10);
    CHECK(v[3] == 40);
}

TEST_CASE("range ctor: elements are converted to T") {
    const char* words[] = {"one", "two", "three"};
    mystl::vector<std::string> v(std::begin(words), std::end(words));  // const char* -> string
    REQUIRE(v.size() == 3);
    CHECK(v[2] == "three");
}

TEST_CASE("range ctor: empty range") {
    const std::list<int> empty;
    mystl::vector<int> v(empty.begin(), empty.end());
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 0);
}

TEST_CASE("range ctor: exception while copying leaks nothing") {
    Tracked::reset();
    {
        std::list<Tracked> source;
        for (int i = 0; i < 6; ++i) {
            source.emplace_back(i);
        }
        Tracked::throw_on_copy_after = 4;
        CHECK_THROWS_AS(mystl::vector<Tracked>(source.begin(), source.end()), std::runtime_error);
        Tracked::throw_on_copy_after = -1;
        CHECK(Tracked::live() == 6);   // yalnız list-in elementləri
    }
    CHECK(Tracked::live() == 0);
}

// ---------------------------------------------------------------------------
// initializer_list
// ---------------------------------------------------------------------------

TEST_CASE("initializer_list ctor") {
    mystl::vector<int> v{1, 2, 3};
    REQUIRE(v.size() == 3);
    CHECK(v[2] == 3);

    mystl::vector<std::string> s = {"a", "b"};
    CHECK(s[1] == "b");

    mystl::vector<int> empty = {};
    CHECK(empty.size() == 0);
}

TEST_CASE("nested initializer_list") {
    mystl::vector<mystl::vector<int>> grid{{1, 2}, {3}, {}};
    REQUIRE(grid.size() == 3);
    CHECK(grid[0].size() == 2);
    CHECK(grid[1][0] == 3);
    CHECK(grid[2].size() == 0);
}

TEST_CASE("class template argument deduction (CTAD)") {
    mystl::vector v{1, 2, 3};
    CHECK(std::is_same_v<decltype(v), mystl::vector<int>>);

    const std::list<double> list{1.5, 2.5};
    mystl::vector w(list.begin(), list.end());
    CHECK(std::is_same_v<decltype(w), mystl::vector<double>>);
}

// ---------------------------------------------------------------------------
// operator=(initializer_list) və assign
// ---------------------------------------------------------------------------

TEST_CASE("operator=(initializer_list)") {
    mystl::vector<int> v{1, 2, 3, 4, 5};
    v = {9, 8};
    REQUIRE(v.size() == 2);
    CHECK(v[0] == 9);
    CHECK(v[1] == 8);
}

TEST_CASE("assign(count, value)") {
    mystl::vector<int> v{1, 2, 3};
    v.assign(5, 42);
    REQUIRE(v.size() == 5);
    CHECK(v[4] == 42);

    v.assign(0, 1);
    CHECK(v.size() == 0);
}

TEST_CASE("assign(count, value) where value is an element of the vector") {
    mystl::vector<std::string> v{"keep me, I am long enough for the heap", "b", "c"};
    v.assign(4, v[0]);  // v[0] köhnə məzmundandır
    REQUIRE(v.size() == 4);
    for (const auto& s : v) {
        CHECK(s == "keep me, I am long enough for the heap");
    }
}

TEST_CASE("assign(first, last) and assign(initializer_list)") {
    mystl::vector<int> v{1};
    const std::list<int> list{4, 5, 6};
    v.assign(list.begin(), list.end());
    REQUIRE(v.size() == 3);
    CHECK(v[2] == 6);

    v.assign({7, 7});
    REQUIRE(v.size() == 2);
    CHECK(v[1] == 7);
}

TEST_CASE("assign: exception leaves the vector unchanged") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v;
        v.push_back(Tracked(1));
        v.push_back(Tracked(2));
        const Tracked prototype(9);

        Tracked::throw_on_copy_after = 2;
        CHECK_THROWS_AS(v.assign(5, prototype), std::runtime_error);
        Tracked::throw_on_copy_after = -1;

        REQUIRE(v.size() == 2);    // strong guarantee
        CHECK(v[1].value == 2);
    }
    CHECK(Tracked::live() == 0);
}
