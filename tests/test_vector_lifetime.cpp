// A4: obyektlərin ömrü — [qurulmuş | xam] yaddaş modelinin testləri.

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

using test::Tracked;

namespace {

mystl::vector<Tracked> make_tracked(int n) {
    mystl::vector<Tracked> v;
    for (int i = 0; i < n; ++i) {
        v.push_back(Tracked(i));
    }
    return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// Default constructor tələbi yoxdur
// ---------------------------------------------------------------------------

TEST_CASE("lifetime: type without a default constructor works") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v;     // v3-də bu sətir kompilyasiya olunmurdu
        v.push_back(Tracked(1));
        v.push_back(Tracked(2));
        v.push_back(Tracked(3));
        REQUIRE(v.size() == 3);
        CHECK(v[2].value == 3);
    }
    CHECK(Tracked::live() == 0);
}

// ---------------------------------------------------------------------------
// reserve
// ---------------------------------------------------------------------------

TEST_CASE("reserve: creates no objects") {
    Tracked::reset();
    mystl::vector<Tracked> v;
    v.reserve(100);
    CHECK(v.capacity() == 100);
    CHECK(v.size() == 0);
    CHECK(Tracked::constructed == 0);  // new T[100] olsaydı, 100 olardı
}

TEST_CASE("reserve: smaller or equal value does nothing") {
    mystl::vector<int> v;
    v.reserve(10);
    v.reserve(5);
    CHECK(v.capacity() == 10);
    v.reserve(10);
    CHECK(v.capacity() == 10);
}

TEST_CASE("reserve: keeps existing elements and destroys the old ones") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v = make_tracked(3);
        const int live_before = Tracked::live();
        REQUIRE(live_before == 3);

        v.reserve(50);
        CHECK(v.capacity() == 50);
        REQUIRE(v.size() == 3);
        CHECK(v[0].value == 0);
        CHECK(v[2].value == 2);
        CHECK(Tracked::live() == 3);  // köhnələr dağıdıldı, yenilər quruldu
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("reserve: no reallocation while size stays below capacity") {
    Tracked::reset();
    mystl::vector<Tracked> v;
    v.reserve(10);
    for (int i = 0; i < 10; ++i) {
        v.push_back(Tracked(i));
    }
    CHECK(v.capacity() == 10);
    // 10 müvəqqəti arqument + 10 kopya vektora. Köçürmə olsaydı, kopya çox olardı.
    CHECK(Tracked::copies == 10);
}

// ---------------------------------------------------------------------------
// pop_back
// ---------------------------------------------------------------------------

TEST_CASE("pop_back: destroys exactly one object and keeps capacity") {
    Tracked::reset();
    mystl::vector<Tracked> v = make_tracked(4);
    const std::size_t cap = v.capacity();
    const int live_before = Tracked::live();

    v.pop_back();
    CHECK(v.size() == 3);
    CHECK(v.capacity() == cap);
    CHECK(Tracked::live() == live_before - 1);
}

TEST_CASE("pop_back then push_back reuses the slot") {
    mystl::vector<std::string> v;
    v.push_back("a fairly long string that does not fit in SSO");
    v.pop_back();
    v.push_back("another long string that also needs the heap!!");
    REQUIRE(v.size() == 1);
    CHECK(v[0] == "another long string that also needs the heap!!");
}

// ---------------------------------------------------------------------------
// Balans: qurulan obyektlərin sayı = dağıdılanların sayı
// ---------------------------------------------------------------------------

TEST_CASE("lifetime: every constructed object is destroyed exactly once") {
    Tracked::reset();
    {
        mystl::vector<Tracked> a = make_tracked(10);
        a.pop_back();
        a.pop_back();
        a.reserve(64);
        a.push_back(Tracked(100));

        mystl::vector<Tracked> b = a;  // copy ctor
        b.pop_back();
        mystl::vector<Tracked> c = make_tracked(3);
        c = b;                         // copy assignment
        c = c;                         // self-assignment (alias-sız, warning yoxdur)
        a.swap(c);
        CHECK(Tracked::live() > 0);
    }
    CHECK(Tracked::constructed == Tracked::destroyed);
    CHECK(Tracked::live() == 0);
}

// ---------------------------------------------------------------------------
// Öz elementini əlavə etmək (klassik tələ)
// ---------------------------------------------------------------------------

TEST_CASE("push_back(v[0]) when the vector is full") {
    mystl::vector<std::string> v;
    v.push_back("the first string is long enough to be on the heap");
    v.push_back("b");
    REQUIRE(v.size() == v.capacity());  // dolu: növbəti push_back buffer-i dəyişəcək

    v.push_back(v[0]);  // v[0] köhnə buffer-dədir!
    REQUIRE(v.size() == 3);
    CHECK(v[2] == "the first string is long enough to be on the heap");
    CHECK(v[0] == v[2]);
}

namespace {
// Destructor dəyəri "zəhərləyir". Dağıdılmış obyektdən oxunsa, -1 görünür.
// std::string ilə bu bug Windows-da çox vaxt təsadüfən gizli qalır: azad
// edilmiş yaddaş hələ köhnə məzmunu saxlayır. Bu tip isə onu dərhal üzə çıxarır.
struct Poisoned {
    int value;
    ~Poisoned() { value = -1; }
};
}  // namespace

TEST_CASE("push_back(v[0]) when full reads v[0] before destroying it") {
    mystl::vector<Poisoned> v;
    v.push_back(Poisoned{42});
    v.push_back(Poisoned{7});
    REQUIRE(v.size() == v.capacity());

    v.push_back(v[0]);
    REQUIRE(v.size() == 3);
    CHECK(v[2].value == 42);  // v3-də -1 idi
}

TEST_CASE("push_back(v.back()) many times with growth") {
    mystl::vector<std::string> v;
    v.push_back("seed string that is long enough to require heap storage");
    for (int i = 0; i < 100; ++i) {
        v.push_back(v[v.size() - 1]);
    }
    REQUIRE(v.size() == 101);
    CHECK(v[100] == "seed string that is long enough to require heap storage");
}

// ---------------------------------------------------------------------------
// Exception safety: kopyalama uğursuz olsa, sızma yoxdur və vektor dəyişmir
// ---------------------------------------------------------------------------

TEST_CASE("exception in push_back during growth: vector is unchanged") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v = make_tracked(4);
        REQUIRE(v.size() == v.capacity());   // növbəti push_back böyüdəcək
        const int live_before = Tracked::live();

        // push_back: 1) yeni elementin kopyası, 2) v[0], 3) v[1] -> exception
        Tracked::throw_on_copy_after = 2;
        CHECK_THROWS_AS(v.push_back(Tracked(99)), std::runtime_error);
        Tracked::throw_on_copy_after = -1;

        CHECK(v.size() == 4);
        CHECK(v.capacity() == 4);
        for (int i = 0; i < 4; ++i) {
            CHECK(v[static_cast<std::size_t>(i)].value == i);
        }
        CHECK(Tracked::live() == live_before);  // yarımçıq qurulanlar dağıdılıb
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("exception in push_back while copying the new element itself") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v = make_tracked(2);
        Tracked::throw_on_copy_after = 0;  // ilk kopya: yeni elementin özü
        CHECK_THROWS_AS(v.push_back(Tracked(5)), std::runtime_error);
        Tracked::throw_on_copy_after = -1;
        CHECK(v.size() == 2);
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("exception in reserve: vector is unchanged") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v = make_tracked(5);
        const std::size_t cap = v.capacity();
        Tracked::throw_on_copy_after = 3;
        CHECK_THROWS_AS(v.reserve(100), std::runtime_error);
        Tracked::throw_on_copy_after = -1;

        CHECK(v.capacity() == cap);
        REQUIRE(v.size() == 5);
        CHECK(v[4].value == 4);
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("exception in copy constructor: nothing leaks") {
    Tracked::reset();
    {
        const mystl::vector<Tracked> a = make_tracked(5);
        const int live_before = Tracked::live();

        Tracked::throw_on_copy_after = 3;
        CHECK_THROWS_AS([&] { mystl::vector<Tracked> b = a; }(), std::runtime_error);
        Tracked::throw_on_copy_after = -1;

        CHECK(Tracked::live() == live_before);
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("exception in copy assignment: target is unchanged") {
    Tracked::reset();
    {
        const mystl::vector<Tracked> source = make_tracked(6);
        mystl::vector<Tracked> target = make_tracked(2);

        Tracked::throw_on_copy_after = 4;
        CHECK_THROWS_AS(target = source, std::runtime_error);
        Tracked::throw_on_copy_after = -1;

        REQUIRE(target.size() == 2);   // copy-and-swap: strong guarantee
        CHECK(target[1].value == 1);
    }
    CHECK(Tracked::live() == 0);
}

// ---------------------------------------------------------------------------
// Hizalama (alignment)
// ---------------------------------------------------------------------------

namespace {
struct alignas(64) CacheLine {
    explicit CacheLine(int v) : value(v) {}
    int value;
};
}  // namespace

TEST_CASE("over-aligned type gets correctly aligned storage") {
    mystl::vector<CacheLine> v;
    for (int i = 0; i < 20; ++i) {
        v.push_back(CacheLine(i));
        const auto address = reinterpret_cast<std::uintptr_t>(&v[0]);
        CHECK(address % 64 == 0);
    }
    CHECK(v[19].value == 19);
}

TEST_CASE("allocation size overflow is detected") {
    mystl::vector<CacheLine> v;  // sizeof(CacheLine) == 64
    const std::size_t too_many = static_cast<std::size_t>(-1) / 64 + 1;
    CHECK_THROWS_AS(v.reserve(too_many), std::bad_array_new_length);
    CHECK(v.capacity() == 0);
}
