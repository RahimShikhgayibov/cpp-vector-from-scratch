// Faza C: differential test.
//
// Eyni təsadüfi əməliyyat ardıcıllığını həm mystl::vector, həm std::vector
// üzərində icra edirik və HƏR addımdan sonra nəticələri müqayisə edirik.
// std::vector "etalon"dur (reference implementation): fərq varsa, səhv bizdədir.
//
// Əl ilə düşünülməmiş ardıcıllıqları (məs. "insert, sonra shrink_to_fit, sonra
// özünə aliaslı push_back") avtomatik yoxlayır.
//
// Ayrıca proqramdır (əsas testlərdən uzun çəkir): build/debug/differential.exe
// Parametrlər:  --steps=N  (default 100000)   --seeds=K  (default 10)

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <mystl/vector.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <list>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_steps = 100'000;
int g_seeds = 10;

// --- Təsadüfi dəyərlər -----------------------------------------------------

template <typename T>
T random_value(std::mt19937& rng);

template <>
int random_value<int>(std::mt19937& rng) {
    return static_cast<int>(rng() % 100);  // kiçik aralıq: təkrarlanan dəyərlər (erase üçün)
}

template <>
std::string random_value<std::string>(std::mt19937& rng) {
    const unsigned n = rng() % 100;
    if (rng() % 2 == 0) {
        return "s" + std::to_string(n);  // qısa: SSO
    }
    // uzun: heap-də yaşayır, kopyalama/köçürmə səhvlərini üzə çıxarır
    return "a long string that does not fit into the SSO buffer #" + std::to_string(n);
}

// --- Müqayisə ----------------------------------------------------------------

template <typename T>
bool same(const mystl::vector<T>& mine, const std::vector<T>& ref) {
    if (mine.size() != ref.size() || mine.empty() != ref.empty()) {
        return false;
    }
    if (mine.size() > mine.capacity()) {
        return false;  // invariant
    }
    if (static_cast<std::size_t>(mine.end() - mine.begin()) != mine.size()) {
        return false;
    }
    return std::equal(mine.begin(), mine.end(), ref.begin());
}

// --- Əməliyyatlar ------------------------------------------------------------

enum Op {
    PushBackCopy, PushBackMove, EmplaceBack, PopBack,
    InsertCopy, InsertMove, InsertN, InsertRange, InsertInitList, Emplace,
    EraseOne, EraseRange, EraseValue,
    Resize, ResizeValue, Reserve, ShrinkToFit, Clear,
    AssignN, AssignRange, AssignInitList,
    CopyAssign, CopyConstruct, MoveAssign, Swap,
    PushBackAlias, InsertAlias, ResizeAlias,
    At, Compare,
    OpCount
};

const char* const kOpNames[OpCount] = {
    "push_back(copy)", "push_back(move)", "emplace_back", "pop_back",
    "insert(copy)", "insert(move)", "insert(n, value)", "insert(range)", "insert({...})", "emplace",
    "erase(pos)", "erase(first, last)", "erase(v, value)",
    "resize(n)", "resize(n, value)", "reserve", "shrink_to_fit", "clear",
    "assign(n, value)", "assign(range)", "assign({...})",
    "copy assignment", "copy constructor", "move assignment", "swap",
    "push_back(v[i])", "insert(pos, v[i])", "resize(n, v[i])",
    "at", "comparison",
};

template <typename T>
class Runner {
public:
    explicit Runner(unsigned seed) : rng_(seed) {}

    // Bir addım: təsadüfi əməliyyat seç və hər iki vektora tətbiq et.
    Op step() {
        // Ölçünü məhdud saxla: böyük vektorlarda hər addımda müqayisə yavaşdır,
        // kiçik vektorlarda isə kənar hallar (boş, bir element) daha tez-tez olur.
        if (ref_.size() > 300) {
            const auto half = static_cast<std::ptrdiff_t>(ref_.size() / 2);
            mine_.erase(mine_.begin(), mine_.begin() + half);
            ref_.erase(ref_.begin(), ref_.begin() + half);
        }

        const auto op = static_cast<Op>(rng_() % OpCount);
        const T value = random_value<T>(rng_);

        switch (op) {
            case PushBackCopy:
                mine_.push_back(value);
                ref_.push_back(value);
                break;
            case PushBackMove: {
                T a = value, b = value;
                mine_.push_back(std::move(a));
                ref_.push_back(std::move(b));
                break;
            }
            case EmplaceBack:
                mine_.emplace_back(value);
                ref_.emplace_back(value);
                break;
            case PopBack:
                if (!ref_.empty()) {
                    mine_.pop_back();
                    ref_.pop_back();
                }
                break;
            case InsertCopy: {
                const auto i = position(true);
                mine_.insert(mine_.begin() + i, value);
                ref_.insert(ref_.begin() + i, value);
                break;
            }
            case InsertMove: {
                const auto i = position(true);
                T a = value, b = value;
                mine_.insert(mine_.begin() + i, std::move(a));
                ref_.insert(ref_.begin() + i, std::move(b));
                break;
            }
            case InsertN: {
                const auto i = position(true);
                const auto n = static_cast<std::size_t>(rng_() % 6);
                mine_.insert(mine_.begin() + i, n, value);
                ref_.insert(ref_.begin() + i, n, value);
                break;
            }
            case InsertRange: {
                const auto i = position(true);
                const std::list<T> source = random_list();
                mine_.insert(mine_.begin() + i, source.begin(), source.end());
                ref_.insert(ref_.begin() + i, source.begin(), source.end());
                break;
            }
            case InsertInitList: {
                const auto i = position(true);
                const T a = random_value<T>(rng_);
                mine_.insert(mine_.begin() + i, {value, a, value});
                ref_.insert(ref_.begin() + i, {value, a, value});
                break;
            }
            case Emplace: {
                const auto i = position(true);
                mine_.emplace(mine_.begin() + i, value);
                ref_.emplace(ref_.begin() + i, value);
                break;
            }
            case EraseOne:
                if (!ref_.empty()) {
                    const auto i = position(false);
                    mine_.erase(mine_.begin() + i);
                    ref_.erase(ref_.begin() + i);
                }
                break;
            case EraseRange: {
                const auto a = position(true);
                const auto b = a + static_cast<std::ptrdiff_t>(
                                       rng_() % (ref_.size() - static_cast<std::size_t>(a) + 1));
                mine_.erase(mine_.begin() + a, mine_.begin() + b);
                ref_.erase(ref_.begin() + a, ref_.begin() + b);
                break;
            }
            case EraseValue: {
                const auto removed_mine = mystl::erase(mine_, value);
                const auto removed_ref = std::erase(ref_, value);
                REQUIRE(removed_mine == removed_ref);
                break;
            }
            case Resize: {
                const auto n = new_size();
                mine_.resize(n);
                ref_.resize(n);
                break;
            }
            case ResizeValue: {
                const auto n = new_size();
                mine_.resize(n, value);
                ref_.resize(n, value);
                break;
            }
            case Reserve: {
                const auto n = static_cast<std::size_t>(rng_() % 400);
                mine_.reserve(n);
                ref_.reserve(n);
                REQUIRE(mine_.capacity() >= n);
                break;
            }
            case ShrinkToFit:
                mine_.shrink_to_fit();
                ref_.shrink_to_fit();
                REQUIRE(mine_.capacity() == mine_.size());
                break;
            case Clear:
                if (rng_() % 10 == 0) {  // nadir: yoxsa vektor həmişə kiçik qalar
                    mine_.clear();
                    ref_.clear();
                }
                break;
            case AssignN: {
                const auto n = static_cast<std::size_t>(rng_() % 20);
                mine_.assign(n, value);
                ref_.assign(n, value);
                break;
            }
            case AssignRange: {
                const std::list<T> source = random_list();
                mine_.assign(source.begin(), source.end());
                ref_.assign(source.begin(), source.end());
                break;
            }
            case AssignInitList:
                mine_.assign({value, value});
                ref_.assign({value, value});
                break;
            case CopyAssign:
                if (rng_() % 2 == 0) {
                    other_mine_ = mine_;
                    other_ref_ = ref_;
                } else {
                    mine_ = other_mine_;
                    ref_ = other_ref_;
                }
                break;
            case CopyConstruct: {
                const mystl::vector<T> copy(mine_);
                REQUIRE(same(copy, ref_));
                REQUIRE(copy.capacity() == copy.size());
                break;
            }
            case MoveAssign:
                // Köçürülmüş std::vector-un vəziyyəti standartda "etibarlı, amma
                // qeyri-müəyyən"dir. Ona görə mənbələri sonra açıq şəkildə təmizləyirik.
                if (rng_() % 2 == 0) {
                    other_mine_ = std::move(mine_);
                    other_ref_ = std::move(ref_);
                    REQUIRE(mine_.empty());  // bizim vektor üçün dəqiq: boşdur
                    mine_.clear();
                    ref_.clear();
                } else {
                    mine_ = std::move(other_mine_);
                    ref_ = std::move(other_ref_);
                    REQUIRE(other_mine_.empty());
                    other_mine_.clear();
                    other_ref_.clear();
                }
                break;
            case Swap:
                swap(mine_, other_mine_);
                std::swap(ref_, other_ref_);
                break;
            case PushBackAlias:
                if (!ref_.empty()) {
                    const auto i = static_cast<std::size_t>(position(false));
                    mine_.push_back(mine_[i]);
                    ref_.push_back(ref_[i]);
                }
                break;
            case InsertAlias:
                if (!ref_.empty()) {
                    const auto i = static_cast<std::size_t>(position(false));
                    const auto pos = position(true);
                    mine_.insert(mine_.begin() + pos, mine_[i]);
                    ref_.insert(ref_.begin() + pos, ref_[i]);
                }
                break;
            case ResizeAlias:
                if (!ref_.empty()) {
                    const auto i = static_cast<std::size_t>(position(false));
                    const auto n = ref_.size() + rng_() % 10;
                    mine_.resize(n, mine_[i]);
                    ref_.resize(n, ref_[i]);
                }
                break;
            case At: {
                const auto i = static_cast<std::size_t>(rng_() % (ref_.size() + 3));
                if (i < ref_.size()) {
                    REQUIRE(mine_.at(i) == ref_.at(i));
                } else {
                    CHECK_THROWS_AS((void)mine_.at(i), std::out_of_range);
                }
                if (!ref_.empty()) {
                    REQUIRE(mine_.front() == ref_.front());
                    REQUIRE(mine_.back() == ref_.back());
                }
                break;
            }
            case Compare:
                REQUIRE((mine_ == other_mine_) == (ref_ == other_ref_));
                REQUIRE((mine_ < other_mine_) == (ref_ < other_ref_));
                REQUIRE((mine_ >= other_mine_) == (ref_ >= other_ref_));
                break;
            case OpCount:
                break;
        }
        return op;
    }

    bool consistent() const { return same(mine_, ref_) && same(other_mine_, other_ref_); }
    std::size_t size() const { return ref_.size(); }

private:
    // Təsadüfi mövqe: [0, size] (insert üçün) və ya [0, size) (erase üçün).
    std::ptrdiff_t position(bool allow_end) {
        const std::size_t bound = ref_.size() + (allow_end ? 1 : 0);
        return static_cast<std::ptrdiff_t>(rng_() % bound);
    }

    std::size_t new_size() {
        return static_cast<std::size_t>(rng_() % (ref_.size() + 20));
    }

    std::list<T> random_list() {
        std::list<T> result;
        const unsigned n = rng_() % 6;
        for (unsigned i = 0; i < n; ++i) {
            result.push_back(random_value<T>(rng_));
        }
        return result;
    }

    std::mt19937 rng_;
    mystl::vector<T> mine_;
    std::vector<T> ref_;
    mystl::vector<T> other_mine_;  // copy, swap, move və müqayisə üçün ikinci cüt
    std::vector<T> other_ref_;
};

template <typename T>
void run_all_seeds() {
    int op_counts[OpCount] = {};
    std::size_t max_size = 0;

    for (int seed = 1; seed <= g_seeds; ++seed) {
        Runner<T> runner(static_cast<unsigned>(seed));
        for (int step = 0; step < g_steps; ++step) {
            const Op op = runner.step();
            ++op_counts[op];
            max_size = std::max(max_size, runner.size());
            if (!runner.consistent()) {
                // std::string: doctest const char*-ı ünvan kimi çap edir
                FAIL("mismatch with std::vector: seed = " << seed << ", step = " << step
                                                          << ", operation = "
                                                          << std::string(kOpNames[op]));
            }
        }
    }

    const int total = g_steps * g_seeds;
    MESSAGE(total << " operations (" << g_seeds << " seeds x " << g_steps
                  << "), largest size " << max_size << ", every operation used at least "
                  << *std::min_element(std::begin(op_counts), std::end(op_counts)) << " times");
    for (int count : op_counts) {
        CHECK(count > 0);  // hər əməliyyat ən azı bir dəfə yoxlanılıb
    }
}

}  // namespace

TEST_CASE("differential: mystl::vector<int> vs std::vector<int>") {
    run_all_seeds<int>();
}

TEST_CASE("differential: mystl::vector<std::string> vs std::vector<std::string>") {
    run_all_seeds<std::string>();
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--steps=", 8) == 0) g_steps = std::atoi(argv[i] + 8);
        if (std::strncmp(argv[i], "--seeds=", 8) == 0) g_seeds = std::atoi(argv[i] + 8);
    }
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
