// B1: elementə giriş və iterator-lar.

#include "doctest.h"

#include <mystl/vector.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

mystl::vector<int> make_ints(std::initializer_list<int> values) {
    mystl::vector<int> v;
    for (int x : values) {
        v.push_back(x);
    }
    return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// at()
// ---------------------------------------------------------------------------

TEST_CASE("at: returns the element when the index is valid") {
    mystl::vector<int> v = make_ints({10, 20, 30});
    CHECK(v.at(0) == 10);
    CHECK(v.at(2) == 30);
    v.at(1) = 99;            // reference qaytarır
    CHECK(v[1] == 99);
}

TEST_CASE("at: throws std::out_of_range when the index is invalid") {
    mystl::vector<int> v = make_ints({1, 2, 3});
    CHECK_THROWS_AS(v.at(3), std::out_of_range);
    CHECK_THROWS_AS(v.at(1000), std::out_of_range);
    CHECK_THROWS_AS(v.at(static_cast<std::size_t>(-1)), std::out_of_range);
}

TEST_CASE("at: empty vector throws for index 0") {
    mystl::vector<int> v;
    CHECK_THROWS_AS(v.at(0), std::out_of_range);
}

TEST_CASE("at: const version") {
    const mystl::vector<int> v = make_ints({5});
    CHECK(v.at(0) == 5);
    CHECK_THROWS_AS(v.at(1), std::out_of_range);
}

TEST_CASE("at: exception message names the index and the size") {
    mystl::vector<int> v = make_ints({1, 2});
    try {
        (void)v.at(7);
        FAIL("at(7) should have thrown");
    } catch (const std::out_of_range& e) {
        const std::string message = e.what();
        CHECK(message.find('7') != std::string::npos);
        CHECK(message.find('2') != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// front, back, data
// ---------------------------------------------------------------------------

TEST_CASE("front and back") {
    mystl::vector<std::string> v;
    v.push_back("first");
    v.push_back("middle");
    v.push_back("last");
    CHECK(v.front() == "first");
    CHECK(v.back() == "last");

    v.front() = "F";
    v.back() += "!";
    CHECK(v[0] == "F");
    CHECK(v[2] == "last!");

    const auto& cv = v;
    CHECK(cv.front() == "F");
    CHECK(cv.back() == "last!");
}

TEST_CASE("front == back for a single element") {
    mystl::vector<int> v = make_ints({42});
    CHECK(&v.front() == &v.back());
}

TEST_CASE("data: points to the first element, nullptr when nothing allocated") {
    mystl::vector<int> empty;
    CHECK(empty.data() == nullptr);

    mystl::vector<int> v = make_ints({1, 2, 3});
    CHECK(v.data() == &v[0]);
    CHECK(v.data()[2] == 3);   // C massivi kimi istifadə

    const auto& cv = v;
    CHECK(std::is_same_v<decltype(cv.data()), const int*>);
}

// ---------------------------------------------------------------------------
// Iterator-lar
// ---------------------------------------------------------------------------

TEST_CASE("iterator types satisfy the standard concepts") {
    using V = mystl::vector<int>;
    CHECK(std::contiguous_iterator<V::iterator>);
    CHECK(std::contiguous_iterator<V::const_iterator>);
    CHECK(std::random_access_iterator<V::reverse_iterator>);
    CHECK(std::ranges::contiguous_range<V>);
    CHECK(std::ranges::sized_range<V>);
}

TEST_CASE("empty vector: begin == end") {
    mystl::vector<int> v;
    CHECK(v.begin() == v.end());
    CHECK(v.cbegin() == v.cend());
    CHECK(v.rbegin() == v.rend());
}

TEST_CASE("end - begin == size") {
    mystl::vector<int> v = make_ints({1, 2, 3, 4});
    CHECK(v.end() - v.begin() == 4);
    CHECK(std::distance(v.rbegin(), v.rend()) == 4);
}

TEST_CASE("range-based for loop reads and writes") {
    mystl::vector<int> v = make_ints({1, 2, 3});
    int sum = 0;
    for (int x : v) {
        sum += x;
    }
    CHECK(sum == 6);

    for (int& x : v) {
        x *= 10;
    }
    CHECK(v[2] == 30);
}

TEST_CASE("const vector returns const iterators") {
    const mystl::vector<int> v = make_ints({1, 2});
    CHECK(std::is_same_v<decltype(v.begin()), const int*>);
    CHECK(std::is_same_v<decltype(v.cbegin()), const int*>);
    // *v.begin() = 5;  // kompilyasiya olunmaz: const_iterator elementi dəyişə bilmir
}

TEST_CASE("cbegin on a non-const vector is still const") {
    mystl::vector<int> v = make_ints({1});
    CHECK(std::is_same_v<decltype(v.cbegin()), const int*>);
    CHECK(std::is_same_v<decltype(v.begin()), int*>);
}

TEST_CASE("reverse iterators walk backwards") {
    mystl::vector<int> v = make_ints({1, 2, 3, 4});
    mystl::vector<int> reversed;
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        reversed.push_back(*it);
    }
    REQUIRE(reversed.size() == 4);
    CHECK(reversed[0] == 4);
    CHECK(reversed[3] == 1);

    CHECK(*v.rbegin() == 4);
    CHECK(&*v.rbegin() == &v.back());
    CHECK(v.rbegin().base() == v.end());   // rbegin end()-də "dayanır"
    CHECK(*v.crbegin() == 4);
}

// ---------------------------------------------------------------------------
// Standart alqoritmlərlə uyğunluq
// ---------------------------------------------------------------------------

TEST_CASE("works with <algorithm> and <numeric>") {
    mystl::vector<int> v = make_ints({5, 3, 8, 1, 9, 2});

    std::sort(v.begin(), v.end());
    CHECK(std::is_sorted(v.begin(), v.end()));
    CHECK(v.front() == 1);
    CHECK(v.back() == 9);

    CHECK(std::accumulate(v.begin(), v.end(), 0) == 28);
    CHECK(std::find(v.begin(), v.end(), 8) != v.end());
    CHECK(std::find(v.begin(), v.end(), 7) == v.end());
    CHECK(std::binary_search(v.begin(), v.end(), 5));

    std::reverse(v.begin(), v.end());
    CHECK(v.front() == 9);
}

TEST_CASE("works with std::ranges algorithms") {
    mystl::vector<std::string> v;
    v.push_back("pear");
    v.push_back("apple");
    v.push_back("fig");

    std::ranges::sort(v);
    CHECK(v[0] == "apple");
    CHECK(v[2] == "pear");

    auto lengths = v | std::views::transform([](const std::string& s) { return s.size(); });
    CHECK(*std::ranges::max_element(lengths) == 5);
}
