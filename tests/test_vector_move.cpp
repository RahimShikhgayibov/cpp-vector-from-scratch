// A5: move semantics və rule of five.

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using test::Tracked;
using test::TrackedThrowingMove;

namespace {

mystl::vector<int> make_ints(int n) {
    mystl::vector<int> v;
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
    }
    return v;
}

mystl::vector<Tracked> make_tracked(int n) {
    mystl::vector<Tracked> v;
    for (int i = 0; i < n; ++i) {
        v.push_back(Tracked(i));
    }
    return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// Vektorun özünün move əməliyyatları
// ---------------------------------------------------------------------------

TEST_CASE("move ops are noexcept") {
    CHECK(std::is_nothrow_move_constructible_v<mystl::vector<int>>);
    CHECK(std::is_nothrow_move_assignable_v<mystl::vector<int>>);
    CHECK(std::is_nothrow_move_constructible_v<mystl::vector<std::string>>);
}

TEST_CASE("move ctor: steals the buffer, source becomes empty") {
    mystl::vector<int> a = make_ints(5);
    const int* buffer = &a[0];

    mystl::vector<int> b = std::move(a);

    REQUIRE(b.size() == 5);
    CHECK(b[4] == 4);
    CHECK(&b[0] == buffer);   // eyni buffer: heç nə kopyalanmayıb
    CHECK(a.size() == 0);     // mənbə boş və etibarlı vəziyyətdədir
    CHECK(a.capacity() == 0);
}

TEST_CASE("move ctor: O(1), no element is touched") {
    constexpr int n = 1'000'000;
    mystl::vector<int> a = make_ints(n);
    const int* buffer = &a[0];

    mystl::vector<int> b(std::move(a));

    CHECK(&b[0] == buffer);   // 1 000 000 element yerində qaldı
    CHECK(b.size() == static_cast<std::size_t>(n));

    Tracked::reset();
    mystl::vector<Tracked> t = make_tracked(100);
    const int constructed_before = Tracked::constructed;
    mystl::vector<Tracked> u = std::move(t);
    CHECK(Tracked::constructed == constructed_before);  // heç bir element qurulmayıb
    CHECK(u.size() == 100);
}

TEST_CASE("moved-from vector is usable") {
    mystl::vector<std::string> a;
    a.push_back("first");
    mystl::vector<std::string> b = std::move(a);

    a.push_back("again");     // mənbəni yenidən istifadə etmək olar
    REQUIRE(a.size() == 1);
    CHECK(a[0] == "again");
    CHECK(b[0] == "first");
}

TEST_CASE("move assignment: target gets the buffer, old contents destroyed") {
    Tracked::reset();
    {
        mystl::vector<Tracked> target = make_tracked(3);
        mystl::vector<Tracked> source = make_tracked(5);
        const int live_before = Tracked::live();   // 8
        const Tracked* buffer = &source[0];

        target = std::move(source);

        REQUIRE(target.size() == 5);
        CHECK(&target[0] == buffer);
        CHECK(source.size() == 0);
        CHECK(Tracked::live() == live_before - 3);  // target-in köhnə 3 elementi dağıdılıb
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("move assignment: self-move leaves the vector intact") {
    mystl::vector<int> a = make_ints(4);
    // `a = std::move(a);` yazsaq, GCC -Wself-move warning-i verir.
    // Real proqramda self-move adətən alias vasitəsilə olur.
    mystl::vector<int>& alias = a;
    a = std::move(alias);

    REQUIRE(a.size() == 4);
    CHECK(a[3] == 3);
}

TEST_CASE("move assignment from a temporary") {
    mystl::vector<int> a = make_ints(2);
    a = make_ints(7);   // müvəqqəti obyekt: move assignment seçilir
    REQUIRE(a.size() == 7);
    CHECK(a[6] == 6);
}

TEST_CASE("swap: no element is copied or moved") {
    Tracked::reset();
    mystl::vector<Tracked> a = make_tracked(3);
    mystl::vector<Tracked> b = make_tracked(4);
    const int copies = Tracked::copies;
    const int moves = Tracked::moves;
    a.swap(b);
    CHECK(Tracked::copies == copies);
    CHECK(Tracked::moves == moves);
    CHECK(a.size() == 4);
}

// ---------------------------------------------------------------------------
// push_back(T&&)
// ---------------------------------------------------------------------------

TEST_CASE("push_back(rvalue) moves, push_back(lvalue) copies") {
    Tracked::reset();
    mystl::vector<Tracked> v;
    v.reserve(4);  // böyümə köçürmələrini istisna etmək üçün

    v.push_back(Tracked(1));           // müvəqqəti obyekt -> T&&
    CHECK(Tracked::moves == 1);
    CHECK(Tracked::copies == 0);

    Tracked item(2);
    v.push_back(item);                 // lvalue -> const T&
    CHECK(Tracked::copies == 1);
    CHECK(item.value == 2);            // lvalue toxunulmaz qalır

    v.push_back(std::move(item));      // lvalue-ni açıq şəkildə rvalue-yə çevir
    CHECK(Tracked::moves == 2);
    CHECK(item.value == -1);           // mənbə "boşaldılıb" (Tracked-in moved-from işarəsi)
    CHECK(v[2].value == 2);
}

TEST_CASE("push_back(std::move(string)) keeps the contents") {
    mystl::vector<std::string> v;
    std::string s = "a long string that lives on the heap, not in SSO buffer";
    v.push_back(std::move(s));
    REQUIRE(v.size() == 1);
    CHECK(v[0] == "a long string that lives on the heap, not in SSO buffer");
}

TEST_CASE("push_back(std::move(v[0])) when full") {
    mystl::vector<std::string> v;
    v.push_back("the first string is long enough to be on the heap");
    v.push_back("b");
    REQUIRE(v.size() == v.capacity());

    v.push_back(std::move(v[0]));      // köhnə buffer-dəki elementdən köçür
    REQUIRE(v.size() == 3);
    CHECK(v[2] == "the first string is long enough to be on the heap");
    CHECK(v[1] == "b");
}

// ---------------------------------------------------------------------------
// Böyümə: move_if_noexcept
// ---------------------------------------------------------------------------

TEST_CASE("growth moves elements when the move ctor is noexcept") {
    Tracked::reset();
    mystl::vector<Tracked> v = make_tracked(8);
    REQUIRE(v.size() == v.capacity());
    const int copies_before = Tracked::copies;
    const int moves_before = Tracked::moves;

    v.push_back(Tracked(8));          // böyümə: 8 köhnə + 1 yeni element

    CHECK(Tracked::copies == copies_before);        // heç bir kopya
    CHECK(Tracked::moves == moves_before + 8 + 1);  // 8 köhnə köçürüldü + yeni element
}

TEST_CASE("growth copies elements when the move ctor is NOT noexcept") {
    TrackedThrowingMove::reset();
    mystl::vector<TrackedThrowingMove> v;
    for (int i = 0; i < 8; ++i) {
        v.push_back(TrackedThrowingMove(i));
    }
    REQUIRE(v.size() == v.capacity());
    const int copies_before = TrackedThrowingMove::copies;

    v.push_back(TrackedThrowingMove(8));

    // strong guarantee üçün vektor köçürmək əvəzinə kopyalamağı seçdi
    CHECK(TrackedThrowingMove::copies == copies_before + 8);
}

TEST_CASE("reserve moves elements when the move ctor is noexcept") {
    Tracked::reset();
    mystl::vector<Tracked> v = make_tracked(5);
    const int copies_before = Tracked::copies;
    v.reserve(100);
    CHECK(Tracked::copies == copies_before);
    REQUIRE(v.size() == 5);
    CHECK(v[4].value == 4);
}

// ---------------------------------------------------------------------------
// Yalnız köçürülə bilən tiplər (move-only)
// ---------------------------------------------------------------------------

TEST_CASE("vector<std::unique_ptr<int>> works") {
    mystl::vector<std::unique_ptr<int>> v;
    for (int i = 0; i < 100; ++i) {
        v.push_back(std::make_unique<int>(i));   // çoxlu böyümə: hamısı köçürülür
    }
    REQUIRE(v.size() == 100);
    for (int i = 0; i < 100; ++i) {
        REQUIRE(v[static_cast<std::size_t>(i)] != nullptr);
        CHECK(*v[static_cast<std::size_t>(i)] == i);
    }

    v.reserve(1000);
    CHECK(*v[99] == 99);

    auto p = std::make_unique<int>(500);
    v.push_back(std::move(p));
    CHECK(p == nullptr);                        // sahiblik vektora keçdi
    CHECK(*v[100] == 500);

    v.pop_back();                               // unique_ptr dağıdılır, int silinir
    CHECK(v.size() == 100);

    mystl::vector<std::unique_ptr<int>> w = std::move(v);   // vektorun özü də köçürülür
    CHECK(*w[0] == 0);
    CHECK(v.size() == 0);
}

// ---------------------------------------------------------------------------
// vector<vector<T>>: daxili vektorlar böyümə zamanı köçürülür
// ---------------------------------------------------------------------------

TEST_CASE("vector<vector<int>> growth moves inner vectors (no deep copy)") {
    mystl::vector<mystl::vector<int>> outer;
    outer.push_back(make_ints(1000));
    const int* inner_buffer = &outer[0][0];

    for (int i = 0; i < 100; ++i) {
        outer.push_back(make_ints(10));         // xarici vektor dəfələrlə böyüyür
    }

    // Daxili vektorun buffer-i yerindədir: köçürülüb, dərin kopyalanmayıb.
    // (vector-un move constructor-u noexcept olduğu üçün mümkündür.)
    CHECK(&outer[0][0] == inner_buffer);
    CHECK(outer[0][999] == 999);
}
