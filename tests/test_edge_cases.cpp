// Faza C: yol xəritəsindəki "mütləq yoxlanacaq edge case-lər" siyahısı.
//
// Bu halların çoxu digər fayllarda da yoxlanılır. Burada isə siyahı bir yerdə,
// yol xəritəsindəki ardıcıllıqla toplanıb ki, hesabatda "edge case -> test"
// uyğunluğu aydın görünsün. Hər TEST_CASE-in adı "edge:" ilə başlayır:
//   build/debug/tests.exe -tc="edge:*"

#include "doctest.h"
#include "test_types.hpp"

#include <mystl/vector.hpp>

#include <cstddef>
#include <iterator>
#include <list>
#include <stdexcept>
#include <string>
#include <utility>

using test::Tracked;
using test::TrackedThrowingMove;

namespace {
const std::string kLong = "a string long enough to live on the heap, not in SSO";
}

// 1. Boş vector -------------------------------------------------------------

TEST_CASE("edge: empty vector") {
    mystl::vector<int> v;
    CHECK_THROWS_AS((void)v.at(0), std::out_of_range);
    CHECK(v.begin() == v.end());
    CHECK(v.rbegin() == v.rend());
    CHECK(v.empty());
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 0);
    CHECK(v.data() == nullptr);

    int visited = 0;
    for (int x : v) visited += x + 1;
    CHECK(visited == 0);

    v.clear();                       // boş vektorda heç nə etmir
    v.erase(v.begin(), v.end());     // boş aralıq
    v.resize(0);
    v.shrink_to_fit();
    CHECK(v.empty());
}

// 2. Self-assignment ---------------------------------------------------------

TEST_CASE("edge: self-assignment v = v") {
    mystl::vector<std::string> v{kLong, "b"};
    v = v;                                        // birbaşa
    CHECK(v == mystl::vector<std::string>{kLong, "b"});

    mystl::vector<std::string>& alias = v;
    v = std::move(alias);                         // self-move
    CHECK(v == mystl::vector<std::string>{kLong, "b"});

    v.assign(v.begin(), v.end());                 // DİQQƏT: std::vector-da bu UB-dir,
                                                  // bizdə "qur, sonra swap" sayəsində işləyir
    CHECK(v.size() == 2);
}

// 3. Öz elementini əlavə etmək (resize zamanı klassik tələ) ------------------

TEST_CASE("edge: v.push_back(v[0])") {
    SUBCASE("vector is full: reallocation") {
        mystl::vector<std::string> v{kLong};
        REQUIRE(v.size() == v.capacity());
        v.push_back(v[0]);
        CHECK(v[1] == kLong);
    }
    SUBCASE("vector has room") {
        mystl::vector<std::string> v{kLong};
        v.reserve(4);
        v.push_back(v[0]);
        CHECK(v[1] == kLong);
    }
}

TEST_CASE("edge: v.insert(v.begin(), v.back())") {
    SUBCASE("vector is full: reallocation") {
        mystl::vector<std::string> v{"a", kLong};
        REQUIRE(v.size() == v.capacity());
        v.insert(v.begin(), v.back());
        CHECK(v == mystl::vector<std::string>{kLong, "a", kLong});
    }
    SUBCASE("vector has room: elements shift under the reference") {
        mystl::vector<std::string> v{"a", kLong};
        v.reserve(8);
        v.insert(v.begin(), v.back());
        CHECK(v == mystl::vector<std::string>{kLong, "a", kLong});
    }
}

// 4. insert və erase: əvvəldə, ortada, sonda, boş aralıq ----------------------

TEST_CASE("edge: insert at begin, middle, end, and an empty range") {
    mystl::vector<int> v{2, 4};
    v.insert(v.begin(), 1);
    v.insert(v.begin() + 2, 3);
    v.insert(v.end(), 5);
    CHECK(v == mystl::vector<int>{1, 2, 3, 4, 5});

    const std::list<int> none;
    for (auto pos : {v.begin(), v.begin() + 2, v.end()}) {
        const auto it = v.insert(pos, none.begin(), none.end());
        CHECK(it == pos);
    }
    v.insert(v.begin() + 1, 0, 42);              // 0 kopya
    v.insert(v.end(), {});                       // boş initializer_list
    CHECK(v == mystl::vector<int>{1, 2, 3, 4, 5});

    mystl::vector<int> empty;
    const std::list<int> some{7, 8};
    empty.insert(empty.end(), some.begin(), some.end());   // boş vektorun sonuna
    CHECK(empty == mystl::vector<int>{7, 8});
}

TEST_CASE("edge: erase at begin, middle, end, and an empty range") {
    mystl::vector<int> v{0, 1, 2, 3, 4, 5};
    CHECK(*v.erase(v.begin()) == 1);
    CHECK(*v.erase(v.begin() + 2) == 4);

    // DİQQƏT: CHECK(v.erase(v.end() - 1) == v.end()) YAZMAQ OLMAZ.
    // == operatorunun iki tərəfinin hesablanma sırası təyin olunmayıb: GCC sağdakı
    // v.end()-i erase-dən ƏVVƏL hesabladı və köhnə (artıq etibarsız) end() ilə
    // müqayisə etdi. Bu, iterator invalidation-un özüdür. Düzgün yol: iki ayrı ifadə.
    const auto after_last = v.erase(v.end() - 1);
    CHECK(after_last == v.end());
    CHECK(v == mystl::vector<int>{1, 2, 4});

    for (auto pos : {v.begin(), v.begin() + 1, v.end()}) {
        CHECK(v.erase(pos, pos) == pos);         // boş aralıq: heç nə silinmir
    }
    CHECK(v == mystl::vector<int>{1, 2, 4});
}

// 5. reserve cari ölçüdən kiçik dəyərlə ---------------------------------------

TEST_CASE("edge: reserve with a value smaller than size and capacity") {
    mystl::vector<int> v{1, 2, 3, 4, 5};
    const auto cap = v.capacity();
    const int* buffer = v.data();
    v.reserve(2);       // size-dan kiçik
    v.reserve(0);
    v.reserve(cap);     // bərabər
    CHECK(v.capacity() == cap);
    CHECK(v.data() == buffer);
    CHECK(v == mystl::vector<int>{1, 2, 3, 4, 5});
}

// 6. resize yuxarı və aşağı, shrink_to_fit boş vektorda ---------------------

TEST_CASE("edge: resize up and down") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v;
        v.resize(5, Tracked(1));
        CHECK(v.size() == 5);
        v.resize(2, Tracked(9));
        CHECK(v.size() == 2);
        CHECK(v[1].value == 1);
        v.resize(4, Tracked(7));
        CHECK(v[3].value == 7);
        v.resize(0, Tracked(0));
        CHECK(v.empty());
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("edge: shrink_to_fit on empty vectors") {
    mystl::vector<int> never_allocated;
    never_allocated.shrink_to_fit();
    CHECK(never_allocated.capacity() == 0);
    CHECK(never_allocated.data() == nullptr);

    mystl::vector<int> emptied{1, 2, 3};
    emptied.clear();
    emptied.shrink_to_fit();
    CHECK(emptied.capacity() == 0);
    CHECK(emptied.data() == nullptr);
}

// 7. Move-dan sonra mənbə vektorun vəziyyəti -----------------------------------

TEST_CASE("edge: moved-from vector is empty and usable") {
    mystl::vector<std::string> source{kLong, "b"};
    mystl::vector<std::string> target = std::move(source);
    CHECK(source.empty());
    CHECK(source.capacity() == 0);
    CHECK(source.begin() == source.end());
    CHECK_THROWS_AS((void)source.at(0), std::out_of_range);

    source.push_back("reused");
    CHECK(source == mystl::vector<std::string>{"reused"});

    mystl::vector<std::string> other{"x"};
    other = std::move(target);
    CHECK(target.empty());
    target = {"again"};
    CHECK(target.size() == 1);
}

// ---------------------------------------------------------------------------
// Coverage-də tapılan boşluqlar (scripts/coverage.ps1)
// ---------------------------------------------------------------------------

namespace {

// İstənilən iterator-u "yalnız bir dəfə keçilə bilən" input iterator kimi göstərir.
// insert(pos, first, last)-in input iterator budağını yoxlamaq üçün.
template <typename It>
class InputOnly {
public:
    using iterator_concept = std::input_iterator_tag;
    using value_type = std::iter_value_t<It>;
    using difference_type = std::ptrdiff_t;

    InputOnly() = default;
    explicit InputOnly(It it) : it_(it) {}
    decltype(auto) operator*() const { return *it_; }
    InputOnly& operator++() { ++it_; return *this; }
    void operator++(int) { ++it_; }
    bool operator==(const InputOnly& other) const { return it_ == other.it_; }

private:
    It it_{};
};

}  // namespace

TEST_CASE("edge: InputOnly really is just an input iterator") {
    using It = InputOnly<std::list<int>::iterator>;
    CHECK(std::input_iterator<It>);
    CHECK_FALSE(std::forward_iterator<It>);
}

TEST_CASE("edge: exception during insert from an input iterator rolls back") {
    Tracked::reset();
    {
        mystl::vector<Tracked> v;
        v.reserve(16);                 // yer var: yerində əlavə yolu
        v.emplace_back(1);
        v.emplace_back(2);
        std::list<Tracked> source;
        for (int i = 10; i < 15; ++i) source.emplace_back(i);
        using It = InputOnly<std::list<Tracked>::iterator>;

        Tracked::throw_on_copy_after = 3;   // 4-cü element atır
        CHECK_THROWS_AS(v.insert(v.begin() + 1, It(source.begin()), It(source.end())),
                        std::runtime_error);
        Tracked::throw_on_copy_after = -1;

        REQUIRE(v.size() == 2);              // əlavə olunan 3 element geri alındı
        CHECK(v[0].value == 1);
        CHECK(v[1].value == 2);
        CHECK(Tracked::live() == 2 + 5);
    }
    CHECK(Tracked::live() == 0);
}

TEST_CASE("edge: insert from an input iterator in the middle") {
    std::list<int> source{7, 8, 9};
    using It = InputOnly<std::list<int>::iterator>;
    mystl::vector<int> v{1, 2};
    v.insert(v.begin() + 1, It(source.begin()), It(source.end()));
    CHECK(v == mystl::vector<int>{1, 7, 8, 9, 2});
}

TEST_CASE("edge: exception in shrink_to_fit leaves the vector unchanged") {
    TrackedThrowingMove::reset();
    {
        mystl::vector<TrackedThrowingMove> v;
        v.reserve(10);
        for (int i = 0; i < 4; ++i) v.emplace_back(i);

        TrackedThrowingMove::throw_on_copy_after = 2;
        CHECK_THROWS_AS(v.shrink_to_fit(), std::runtime_error);
        TrackedThrowingMove::throw_on_copy_after = -1;

        CHECK(v.capacity() == 10);
        REQUIRE(v.size() == 4);
        CHECK(v[3].value == 3);
    }
    CHECK(TrackedThrowingMove::live() == 0);
}

TEST_CASE("edge: inserting or resizing beyond max_size throws length_error") {
    mystl::vector<int> v{1, 2, 3};
    CHECK_THROWS_AS(v.insert(v.begin(), v.max_size(), 0), std::length_error);
    CHECK_THROWS_AS(v.resize(v.max_size() + 1), std::length_error);
    CHECK(v == mystl::vector<int>{1, 2, 3});
}
