// B3: tutum — empty, max_size, reserve, shrink_to_fit, resize.

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using test::Tracked;
using test::TrackedThrowingMove;

// ---------------------------------------------------------------------------
// empty, max_size
// ---------------------------------------------------------------------------

TEST_CASE("empty") {
    mystl::vector<int> v;
    CHECK(v.empty());
    v.push_back(1);
    CHECK_FALSE(v.empty());
    v.pop_back();
    CHECK(v.empty());
}

TEST_CASE("max_size") {
    const auto ptrdiff_max = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    CHECK(mystl::vector<char>().max_size() == ptrdiff_max);
    CHECK(mystl::vector<std::uint64_t>().max_size() == ptrdiff_max / 8);
}

TEST_CASE("reserve beyond max_size throws length_error and changes nothing") {
    mystl::vector<int> v{1, 2, 3};
    const std::size_t cap = v.capacity();
    CHECK_THROWS_AS(v.reserve(v.max_size() + 1), std::length_error);
    CHECK(v.capacity() == cap);
    CHECK(v.size() == 3);
}

// ---------------------------------------------------------------------------
// shrink_to_fit
// ---------------------------------------------------------------------------

TEST_CASE("shrink_to_fit: capacity becomes size, elements kept") {
    mystl::vector<std::string> v;
    for (int i = 0; i < 5; ++i) {
        v.push_back("element number " + std::to_string(i) + " which is long enough for the heap");
    }
    REQUIRE(v.capacity() == 8);
    v.shrink_to_fit();
    CHECK(v.capacity() == 5);
    REQUIRE(v.size() == 5);
    CHECK(v[4] == "element number 4 which is long enough for the heap");
}

TEST_CASE("shrink_to_fit: empty vector releases its memory") {
    mystl::vector<int> v;
    v.reserve(100);
    v.shrink_to_fit();
    CHECK(v.capacity() == 0);
    CHECK(v.data() == nullptr);
    v.push_back(1);             // istifadəyə yararlıdır
    CHECK(v[0] == 1);
}

TEST_CASE("shrink_to_fit: already tight vector is not reallocated") {
    mystl::vector<int> v{1, 2, 3};
    const int* before = v.data();
    v.shrink_to_fit();
    CHECK(v.data() == before);
}

TEST_CASE("shrink_to_fit: moves, does not copy") {
    Tracked::reset();
    mystl::vector<Tracked> v;
    v.reserve(10);
    v.push_back(Tracked(1));
    v.push_back(Tracked(2));
    const int copies = Tracked::copies;
    v.shrink_to_fit();
    CHECK(Tracked::copies == copies);
    CHECK(v.capacity() == 2);
}

// ---------------------------------------------------------------------------
// resize
// ---------------------------------------------------------------------------

TEST_CASE("resize: grow with value-initialized elements") {
    mystl::vector<int> v{1, 2};
    v.resize(5);
    REQUIRE(v.size() == 5);
    CHECK(v[1] == 2);
    CHECK(v[2] == 0);
    CHECK(v[4] == 0);
}

TEST_CASE("resize: shrink destroys the tail, keeps capacity") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v;
        for (int i = 0; i < 6; ++i) {
            v.push_back(Tracked(i));
        }
        const std::size_t cap = v.capacity();
        const int live = Tracked::live();

        v.resize(2, Tracked(0));  // value istifadə olunmur, yalnız kiçilmə
        CHECK(v.size() == 2);
        CHECK(v.capacity() == cap);
        CHECK(Tracked::live() == live - 4);
        CHECK(v[1].value == 1);
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("resize: same size does nothing, resize(0) empties") {
    mystl::vector<int> v{1, 2, 3};
    const int* before = v.data();
    v.resize(3);
    CHECK(v.data() == before);
    CHECK(v.size() == 3);

    v.resize(0);
    CHECK(v.empty());
    CHECK(v.capacity() == 3);
}

TEST_CASE("resize(count, value): new elements are copies of value") {
    mystl::vector<std::string> v{"a"};
    v.resize(4, "filler string that does not fit into SSO");
    REQUIRE(v.size() == 4);
    CHECK(v[0] == "a");
    CHECK(v[3] == "filler string that does not fit into SSO");
}

TEST_CASE("resize(count, v[0]) with reallocation") {
    mystl::vector<std::string> v{"the first string, long enough for heap storage"};
    REQUIRE(v.capacity() == 1);
    v.resize(5, v[0]);  // v[0] köhnə buffer-dədir
    REQUIRE(v.size() == 5);
    for (const auto& s : v) {
        CHECK(s == "the first string, long enough for heap storage");
    }
}

TEST_CASE("resize: growth policy") {
    mystl::vector<int> v{1, 2, 3, 4};
    REQUIRE(v.capacity() == 4);
    v.resize(5);
    CHECK(v.capacity() == 8);     // 2× (5-dən böyükdür)
    v.resize(100);
    CHECK(v.capacity() == 100);   // tələb 2×-dən böyükdür: tam tələb
}

TEST_CASE("resize: exception without reallocation leaves the vector unchanged") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v;
        v.reserve(10);
        v.push_back(Tracked(1));
        v.push_back(Tracked(2));
        const Tracked filler(9);

        Tracked::throw_on_copy_after = 3;     // 4-cü yeni element atır
        CHECK_THROWS_AS(v.resize(8, filler), std::runtime_error);
        Tracked::throw_on_copy_after = -1;

        CHECK(v.size() == 2);
        CHECK(v.capacity() == 10);
        CHECK(Tracked::live() == 3);          // v-nin 2 elementi + filler
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("resize: exception with reallocation leaves the vector unchanged") {
    TrackedThrowingMove::reset();
    {
        mystl::vector<TrackedThrowingMove> v;
        v.push_back(TrackedThrowingMove(1));
        v.push_back(TrackedThrowingMove(2));
        REQUIRE(v.capacity() == 2);
        const TrackedThrowingMove filler(9);

        // 5 yeni element (uğurlu), sonra köhnələr KOPYALANIR: 1-ci uğurlu, 2-ci atır
        TrackedThrowingMove::throw_on_copy_after = 6;
        CHECK_THROWS_AS(v.resize(7, filler), std::runtime_error);
        TrackedThrowingMove::throw_on_copy_after = -1;

        CHECK(v.size() == 2);
        CHECK(v.capacity() == 2);
        CHECK(v[1].value == 2);
        CHECK(TrackedThrowingMove::live() == 3);
    }
    CHECK(TrackedThrowingMove::live() == 0);
}
