// B4: modifikatorlar — emplace_back, emplace, insert, erase, clear.

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <list>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using test::Tracked;
using test::TrackedThrowingMove;

namespace {

// Vektorun məzmununu std::vector ilə müqayisə etmək üçün.
template <typename T>
bool same(const mystl::vector<T>& mine, const std::vector<T>& expected) {
    if (mine.size() != expected.size()) {
        return false;
    }
    for (std::size_t i = 0; i < mine.size(); ++i) {
        if (!(mine[i] == expected[i])) {
            return false;
        }
    }
    return true;
}

struct Person {
    Person(std::string n, int a) : name(std::move(n)), age(a) {}
    std::string name;
    int age;
};

const std::string kLong = "a string that is long enough to need heap storage";

}  // namespace

// ---------------------------------------------------------------------------
// emplace_back
// ---------------------------------------------------------------------------

TEST_CASE("emplace_back constructs in place from constructor arguments") {
    mystl::vector<Person> people;
    Person& alice = people.emplace_back("Alice", 30);   // müvəqqəti Person yoxdur
    CHECK(&alice == &people.back());
    CHECK(people[0].name == "Alice");
    CHECK(people[0].age == 30);
}

TEST_CASE("emplace_back: no copies and no moves of the element") {
    Tracked::reset();
    mystl::vector<Tracked> v;
    v.reserve(4);
    v.emplace_back(7);                  // Tracked(int) birbaşa yaddaşda
    CHECK(Tracked::constructed == 1);
    CHECK(Tracked::copies == 0);
    CHECK(Tracked::moves == 0);

    v.push_back(Tracked(8));            // müqayisə: müvəqqəti obyekt + move
    CHECK(Tracked::constructed == 3);
    CHECK(Tracked::moves == 1);
}

TEST_CASE("emplace_back(v[0]) when full") {
    mystl::vector<std::string> v{kLong};
    REQUIRE(v.size() == v.capacity());
    v.emplace_back(v[0]);
    CHECK(v[1] == kLong);
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

TEST_CASE("clear destroys all elements and keeps capacity") {
    Tracked::reset();
    mystl::vector<Tracked> v;
    for (int i = 0; i < 5; ++i) {
        v.emplace_back(i);
    }
    const std::size_t cap = v.capacity();
    v.clear();
    CHECK(v.empty());
    CHECK(v.capacity() == cap);
    CHECK(Tracked::live() == 0);
    v.emplace_back(1);                  // istifadəyə yararlıdır
    CHECK(v.size() == 1);
}

// ---------------------------------------------------------------------------
// insert / emplace: tək element
// ---------------------------------------------------------------------------

TEST_CASE("insert single element at begin, middle and end") {
    mystl::vector<int> v{1, 2, 3};

    auto it = v.insert(v.begin(), 0);
    CHECK(*it == 0);
    CHECK(it == v.begin());

    it = v.insert(v.begin() + 2, 99);
    CHECK(*it == 99);
    it = v.insert(v.end(), 4);
    CHECK(*it == 4);
    CHECK(it == v.end() - 1);

    CHECK(same(v, std::vector<int>{0, 1, 99, 2, 3, 4}));
}

TEST_CASE("insert into an empty vector") {
    mystl::vector<std::string> v;
    v.insert(v.begin(), kLong);
    REQUIRE(v.size() == 1);
    CHECK(v[0] == kLong);
}

TEST_CASE("insert with and without reallocation gives the same result") {
    mystl::vector<int> tight{1, 2, 3, 4};         // capacity 4: yenidən ayırma lazımdır
    mystl::vector<int> roomy{1, 2, 3, 4};
    roomy.reserve(100);                           // yer var
    tight.insert(tight.begin() + 1, 7);
    roomy.insert(roomy.begin() + 1, 7);
    CHECK(same(tight, std::vector<int>{1, 7, 2, 3, 4}));
    CHECK(same(roomy, std::vector<int>{1, 7, 2, 3, 4}));
}

TEST_CASE("insert(v.begin(), v.back()): the classic aliasing trap") {
    SUBCASE("with reallocation") {
        mystl::vector<std::string> v{"x", "y", kLong};
        REQUIRE(v.size() == v.capacity());
        v.insert(v.begin(), v.back());
        CHECK(v[0] == kLong);
        CHECK(v[3] == kLong);
    }
    SUBCASE("without reallocation") {
        mystl::vector<std::string> v{"x", "y", kLong};
        v.reserve(10);
        v.insert(v.begin(), v.back());   // sürüşmə v.back()-in yerini dəyişir
        CHECK(v[0] == kLong);
        CHECK(v[3] == kLong);
    }
}

TEST_CASE("insert(rvalue) moves the argument") {
    Tracked::reset();
    mystl::vector<Tracked> v;
    v.reserve(10);
    v.emplace_back(1);
    v.emplace_back(2);
    v.insert(v.begin(), Tracked(0));
    CHECK(Tracked::copies == 0);
    CHECK(v[0].value == 0);
    CHECK(v[2].value == 2);
}

TEST_CASE("emplace(pos, args...)") {
    mystl::vector<Person> v;
    v.emplace_back("Bob", 40);
    v.emplace(v.begin(), "Alice", 30);
    REQUIRE(v.size() == 2);
    CHECK(v[0].name == "Alice");
    CHECK(v[1].name == "Bob");
}

// ---------------------------------------------------------------------------
// insert: count kopya, aralıq, initializer_list
// ---------------------------------------------------------------------------

TEST_CASE("insert(pos, count, value)") {
    mystl::vector<int> v{1, 2, 3};
    auto it = v.insert(v.begin() + 1, 3, 9);
    CHECK(it == v.begin() + 1);
    CHECK(same(v, std::vector<int>{1, 9, 9, 9, 2, 3}));

    it = v.insert(v.begin() + 2, 0, 5);   // 0 element: heç nə dəyişmir
    CHECK(it == v.begin() + 2);
    CHECK(v.size() == 6);
}

TEST_CASE("insert(pos, count, v.back()) aliasing") {
    SUBCASE("with reallocation") {
        mystl::vector<std::string> v{"a", kLong};
        v.insert(v.begin(), 3, v.back());
        CHECK(v[0] == kLong);
        CHECK(v[2] == kLong);
        CHECK(v[3] == "a");
        CHECK(v[4] == kLong);
    }
    SUBCASE("without reallocation") {
        mystl::vector<std::string> v{"a", kLong};
        v.reserve(20);
        v.insert(v.begin(), 3, v.back());
        CHECK(v[0] == kLong);
        CHECK(v[3] == "a");
        CHECK(v[4] == kLong);
    }
}

TEST_CASE("insert(pos, first, last) from forward and input iterators") {
    mystl::vector<int> v{1, 5};
    const std::list<int> middle{2, 3, 4};
    auto it = v.insert(v.begin() + 1, middle.begin(), middle.end());
    CHECK(*it == 2);
    CHECK(same(v, std::vector<int>{1, 2, 3, 4, 5}));

    std::istringstream input("8 9");
    v.insert(v.end(), std::istream_iterator<int>(input), std::istream_iterator<int>());
    CHECK(same(v, std::vector<int>{1, 2, 3, 4, 5, 8, 9}));

    const std::list<int> empty;
    it = v.insert(v.begin(), empty.begin(), empty.end());
    CHECK(it == v.begin());
    CHECK(v.size() == 7);
}

TEST_CASE("insert(pos, initializer_list)") {
    mystl::vector<std::string> v{"a", "d"};
    v.insert(v.begin() + 1, {"b", "c"});
    CHECK(same(v, std::vector<std::string>{"a", "b", "c", "d"}));
}

// ---------------------------------------------------------------------------
// erase
// ---------------------------------------------------------------------------

TEST_CASE("erase single element: returns iterator to the next one") {
    mystl::vector<int> v{0, 1, 2, 3, 4};

    auto it = v.erase(v.begin() + 2);
    CHECK(*it == 3);
    CHECK(same(v, std::vector<int>{0, 1, 3, 4}));

    it = v.erase(v.begin());
    CHECK(*it == 1);

    it = v.erase(v.end() - 1);
    CHECK(it == v.end());
    CHECK(same(v, std::vector<int>{1, 3}));
}

TEST_CASE("erase range") {
    mystl::vector<int> v{0, 1, 2, 3, 4, 5, 6};

    auto it = v.erase(v.begin() + 1, v.begin() + 3);   // 1, 2
    CHECK(*it == 3);
    CHECK(same(v, std::vector<int>{0, 3, 4, 5, 6}));

    it = v.erase(v.begin() + 2, v.begin() + 2);        // boş aralıq
    CHECK(*it == 4);
    CHECK(v.size() == 5);

    it = v.erase(v.begin() + 3, v.end());              // sonluq
    CHECK(it == v.end());
    CHECK(same(v, std::vector<int>{0, 3, 4}));

    it = v.erase(v.begin(), v.end());                  // hamısı
    CHECK(v.empty());
    CHECK(it == v.end());
}

TEST_CASE("erase destroys exactly the removed number of objects") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v;
        for (int i = 0; i < 10; ++i) {
            v.emplace_back(i);
        }
        v.erase(v.begin() + 2, v.begin() + 5);
        CHECK(Tracked::live() == 7);
        v.erase(v.begin());
        CHECK(Tracked::live() == 6);
        CHECK(v[0].value == 1);
        CHECK(v[1].value == 5);
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("erase-remove idiom") {
    mystl::vector<int> v{1, 2, 3, 2, 4, 2};
    v.erase(std::remove(v.begin(), v.end(), 2), v.end());
    CHECK(same(v, std::vector<int>{1, 3, 4}));
}

// ---------------------------------------------------------------------------
// Exception: ortaya insert zamanı exception sızma yaratmır (basic guarantee)
// ---------------------------------------------------------------------------

TEST_CASE("exception during insert in the middle: no leak, vector stays valid") {
    TrackedThrowingMove::reset();
    {
        mystl::vector<TrackedThrowingMove> v;
        for (int i = 0; i < 4; ++i) {
            v.push_back(TrackedThrowingMove(i));
        }
        const TrackedThrowingMove item(9);

        TrackedThrowingMove::throw_on_copy_after = 1;
        CHECK_THROWS_AS(v.insert(v.begin() + 1, 2, item), std::runtime_error);
        TrackedThrowingMove::throw_on_copy_after = -1;

        // Bu halda (yenidən ayırma) vektor hətta tam əvvəlki kimidir.
        CHECK(v.size() == 4);
        CHECK(TrackedThrowingMove::live() == 5);
    }
    CHECK(TrackedThrowingMove::live() == 0);
}

// ---------------------------------------------------------------------------
// Kiçik differential test: təsadüfi əməliyyatlar, std::vector ilə müqayisə
// (Faza C-də daha böyüyü yazılacaq)
// ---------------------------------------------------------------------------

TEST_CASE("random operations match std::vector") {
    std::mt19937 rng(12345);
    mystl::vector<int> mine;
    std::vector<int> expected;

    auto random_index = [&](std::size_t bound) {
        return std::uniform_int_distribution<std::size_t>(0, bound)(rng);
    };

    for (int step = 0; step < 5000; ++step) {
        const int value = static_cast<int>(rng() % 1000);
        switch (rng() % 8) {
            case 0:
                mine.push_back(value);
                expected.push_back(value);
                break;
            case 1:
                if (!expected.empty()) {
                    mine.pop_back();
                    expected.pop_back();
                }
                break;
            case 2: {
                const std::size_t i = random_index(expected.size());
                mine.insert(mine.begin() + static_cast<std::ptrdiff_t>(i), value);
                expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(i), value);
                break;
            }
            case 3:
                if (!expected.empty()) {
                    const std::size_t i = random_index(expected.size() - 1);
                    mine.erase(mine.begin() + static_cast<std::ptrdiff_t>(i));
                    expected.erase(expected.begin() + static_cast<std::ptrdiff_t>(i));
                }
                break;
            case 4: {
                const std::size_t i = random_index(expected.size());
                const std::size_t n = random_index(5);
                mine.insert(mine.begin() + static_cast<std::ptrdiff_t>(i), n, value);
                expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(i), n, value);
                break;
            }
            case 5: {
                const std::size_t a = random_index(expected.size());
                const std::size_t b = a + random_index(expected.size() - a);
                mine.erase(mine.begin() + static_cast<std::ptrdiff_t>(a),
                           mine.begin() + static_cast<std::ptrdiff_t>(b));
                expected.erase(expected.begin() + static_cast<std::ptrdiff_t>(a),
                               expected.begin() + static_cast<std::ptrdiff_t>(b));
                break;
            }
            case 6: {
                const std::size_t n = random_index(expected.size() + 10);
                mine.resize(n, value);
                expected.resize(n, value);
                break;
            }
            default:
                if (rng() % 50 == 0) {
                    mine.clear();
                    expected.clear();
                }
                break;
        }
        REQUIRE(same(mine, expected));
    }
}
