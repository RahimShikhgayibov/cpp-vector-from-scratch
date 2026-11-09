// B6: peşəkar səviyyə — growth policy, strong guarantee-nin tam yoxlanması,
// const versiyalar, iterator invalidation qaydaları.

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using test::Tracked;
using test::TrackedThrowingMove;

namespace {

template <typename V>
std::vector<std::size_t> capacity_sequence(int pushes) {
    V v;
    std::vector<std::size_t> seen;
    for (int i = 0; i < pushes; ++i) {
        v.push_back(typename V::value_type{});
        if (seen.empty() || seen.back() != v.capacity()) {
            seen.push_back(v.capacity());
        }
    }
    return seen;
}

// Xətti böyümə: +4. Bilərəkdən PİS siyasət — yalnız interfeysin işlədiyini göstərir.
struct plus_four {
    static constexpr std::size_t next(std::size_t capacity) noexcept { return capacity + 4; }
};

}  // namespace

// ---------------------------------------------------------------------------
// Growth policy
// ---------------------------------------------------------------------------

TEST_CASE("default growth policy is doubling") {
    CHECK(std::is_same_v<mystl::vector<int>::growth_type, mystl::growth::doubling>);
    CHECK(capacity_sequence<mystl::vector<int>>(64) ==
          std::vector<std::size_t>{1, 2, 4, 8, 16, 32, 64});
}

TEST_CASE("1.5x growth policy") {
    using V = mystl::vector<int, mystl::growth::one_and_half>;
    // 1 -> max(1 + 0, 2) = 2 -> 3 -> 4 -> 6 -> 9 -> 13 -> 19 -> 28 -> 42 -> 63 -> 94
    CHECK(capacity_sequence<V>(64) ==
          std::vector<std::size_t>{1, 2, 3, 4, 6, 9, 13, 19, 28, 42, 63, 94});
}

TEST_CASE("custom growth policy") {
    using V = mystl::vector<int, plus_four>;
    CHECK(capacity_sequence<V>(13) == std::vector<std::size_t>{4, 8, 12, 16});
}

TEST_CASE("a vector with another policy behaves the same otherwise") {
    mystl::vector<std::string, mystl::growth::one_and_half> v{"a", "b"};
    v.insert(v.begin() + 1, "x");
    v.erase(v.begin());
    CHECK(v == mystl::vector<std::string, mystl::growth::one_and_half>{"x", "b"});
    CHECK(mystl::erase(v, "b") == 1);
}

TEST_CASE("total relocations stay linear for both geometric policies") {
    constexpr int n = 10'000;

    Tracked::reset();
    {
        mystl::vector<Tracked> v;
        for (int i = 0; i < n; ++i) v.emplace_back(i);
    }
    const int moves_2x = Tracked::moves;

    Tracked::reset();
    {
        mystl::vector<Tracked, mystl::growth::one_and_half> v;
        for (int i = 0; i < n; ++i) v.emplace_back(i);
    }
    const int moves_15x = Tracked::moves;

    // Nəzəriyyə (son capacity C olsun, C < factor * n):
    //   2×:   köçürmələr = C/2 + C/4 + ...       < C    < 2n
    //   1.5×: köçürmələr = C/1.5 + C/1.5² + ...  < 2C   < 3n
    // n = 10000 üçün 2×: 1 + 2 + ... + 8192 = 16383 (C = 16384).
    CHECK(moves_2x < 2 * n);
    CHECK(moves_15x < 3 * n);
    CHECK(moves_15x > moves_2x);   // 1.5× daha tez-tez böyüyür
    MESSAGE("relocations for n = 10000:  2x = " << moves_2x << ",  1.5x = " << moves_15x);
}

// ---------------------------------------------------------------------------
// Strong guarantee: HƏR mümkün exception nöqtəsi üçün
// ---------------------------------------------------------------------------

TEST_CASE("push_back: strong guarantee at every possible failure point") {
    // Hər ölçü üçün (vektor dolu, növbəti push_back böyüdəcək) exception-u
    // ardıcıl olaraq hər kopyada atdırırıq: k = 0 (yeni element), 1, 2, ... size.
    for (int size = 0; size <= 16; ++size) {
        for (int k = 0; k <= size; ++k) {
            TrackedThrowingMove::reset();
            {
                mystl::vector<TrackedThrowingMove> v;
                v.reserve(static_cast<std::size_t>(size));
                for (int i = 0; i < size; ++i) v.emplace_back(i);
                const std::size_t cap = v.capacity();
                const TrackedThrowingMove item(100);
                const int live_before = TrackedThrowingMove::live();

                TrackedThrowingMove::throw_on_copy_after = k;
                bool threw = false;
                try {
                    v.push_back(item);       // lvalue: yeni element də kopyalanır
                } catch (const std::runtime_error&) {
                    threw = true;
                }
                TrackedThrowingMove::throw_on_copy_after = -1;

                INFO("size = " << size << ", throw at copy #" << k);
                REQUIRE(threw);              // size + 1 kopya lazımdır, k <= size
                REQUIRE(v.size() == static_cast<std::size_t>(size));
                REQUIRE(v.capacity() == cap);
                for (int i = 0; i < size; ++i) {
                    REQUIRE(v[static_cast<std::size_t>(i)].value == i);
                }
                REQUIRE(TrackedThrowingMove::live() == live_before);
            }
            REQUIRE(TrackedThrowingMove::live() == 0);
        }
    }
}

TEST_CASE("reserve: strong guarantee at every possible failure point") {
    for (int size = 1; size <= 16; ++size) {
        for (int k = 0; k < size; ++k) {
            TrackedThrowingMove::reset();
            {
                mystl::vector<TrackedThrowingMove> v;
                for (int i = 0; i < size; ++i) v.emplace_back(i);
                const std::size_t cap = v.capacity();

                TrackedThrowingMove::throw_on_copy_after = k;
                CHECK_THROWS_AS(v.reserve(cap * 4), std::runtime_error);
                TrackedThrowingMove::throw_on_copy_after = -1;

                INFO("size = " << size << ", throw at copy #" << k);
                REQUIRE(v.capacity() == cap);
                REQUIRE(v.size() == static_cast<std::size_t>(size));
                REQUIRE(v.back().value == size - 1);
            }
            REQUIRE(TrackedThrowingMove::live() == 0);
        }
    }
}

// ---------------------------------------------------------------------------
// const versiyalar: const vektorda bütün oxuma metodları çağırıla bilir
// ---------------------------------------------------------------------------

namespace {

int read_everything(const mystl::vector<int>& v) {
    int sum = v[0] + v.at(0) + v.front() + v.back() + *v.data();
    for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
    for (auto it = v.cbegin(); it != v.cend(); ++it) sum += *it;
    for (auto it = v.rbegin(); it != v.rend(); ++it) sum += *it;
    for (auto it = v.crbegin(); it != v.crend(); ++it) sum += *it;
    sum += static_cast<int>(v.size() + v.capacity()) + (v.empty() ? 1 : 0);
    sum += (v.max_size() > 0) + (v == v) + (v < v);
    return sum;
}

}  // namespace

TEST_CASE("const vector supports every read-only operation") {
    const mystl::vector<int> v{1, 2, 3};
    // v[0] + at(0) + front + back + *data = 1 + 1 + 1 + 3 + 1 = 7
    // dörd dövr: 4 × 6 = 24;  size + capacity = 6;  max_size > 0: 1;  v == v: 1;  v < v: 0
    CHECK(read_everything(v) == 7 + 24 + 6 + 1 + 1);
}

// ---------------------------------------------------------------------------
// Iterator invalidation: README-dəki cədvəlin müşahidə oluna bilən hissəsi
// ---------------------------------------------------------------------------

TEST_CASE("invalidation: push_back without reallocation keeps iterators") {
    mystl::vector<int> v{1, 2, 3};
    v.reserve(10);
    const int* first = &v[0];
    auto it = v.begin() + 1;
    v.push_back(4);
    CHECK(&v[0] == first);
    CHECK(*it == 2);
}

TEST_CASE("invalidation: push_back with reallocation moves the buffer") {
    mystl::vector<int> v{1, 2, 3};
    REQUIRE(v.size() == v.capacity());
    const int* before = v.data();
    v.push_back(4);
    CHECK(v.data() != before);   // köhnə iterator-lar artıq silinmiş yaddaşı göstərir
}

TEST_CASE("invalidation: erase keeps iterators before the erased position") {
    mystl::vector<int> v{0, 1, 2, 3, 4};
    auto before = v.begin() + 1;
    v.erase(v.begin() + 3);
    CHECK(*before == 1);
    CHECK(before == v.begin() + 1);
}

TEST_CASE("invalidation: insert without reallocation keeps iterators before pos") {
    mystl::vector<int> v{0, 1, 2, 3};
    v.reserve(10);
    auto before = v.begin() + 1;
    v.insert(v.begin() + 2, 99);
    CHECK(*before == 1);
}

TEST_CASE("invalidation: swap keeps iterators valid, they now refer to the other vector") {
    mystl::vector<int> a{1, 2};
    mystl::vector<int> b{7, 8, 9};
    auto it = a.begin();
    a.swap(b);
    CHECK(it == b.begin());
    CHECK(*it == 1);
}

TEST_CASE("invalidation: move keeps the buffer, iterators follow the elements") {
    mystl::vector<int> a{1, 2, 3};
    auto it = a.begin() + 2;
    mystl::vector<int> b = std::move(a);
    CHECK(it == b.begin() + 2);
    CHECK(*it == 3);
}
