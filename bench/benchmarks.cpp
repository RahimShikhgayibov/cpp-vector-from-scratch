// Faza D: benchmark-lar və 7 tədqiqat sualı.
//
// İstifadə (yalnız Release build!):
//   cmake --preset release && cmake --build --preset release
//   build/release/benchmarks.exe                 # bütün suallar
//   build/release/benchmarks.exe q1 q3           # seçilmiş suallar
//   build/release/benchmarks.exe --quick         # kiçik n, tez yoxlama
//   build/release/benchmarks.exe --repeats=9 --out=some/dir
//
// Nəticələr CSV-yə yazılır (default: docs/benchmark/data/),
// qrafikləri scripts/plot_benchmarks.py çəkir.

#include "bench_common.hpp"

#include <mystl/vector.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using bench::Csv;
using bench::g_sink;
using bench::log_sizes;
using bench::median_ns;
using bench::median_ns_with_setup;

namespace {

using Mine = mystl::vector<int>;
using Std = std::vector<int>;

// ===========================================================================
// Köməkçi tiplər
// ===========================================================================

// Köçürmə və kopyaları sayan element (yalnız saymaq üçün, vaxt ölçülmür).
struct Counted {
    inline static std::uint64_t moves = 0;
    inline static std::uint64_t copies = 0;
    int value;
    explicit Counted(int v) : value(v) {}
    Counted(const Counted& o) : value(o.value) { ++copies; }
    Counted(Counted&& o) noexcept : value(o.value) { ++moves; }
    Counted& operator=(const Counted&) = default;
    Counted& operator=(Counted&&) noexcept = default;
};

// 40 simvol: SSO-ya sığmır, hər sətir heap-də yaşayır.
constexpr const char* kText = "a benchmark string that lives on the heap";

// Move constructor-u noexcept OLAN sətir (std::string kimi).
struct NoexceptMoveString {
    std::string s;
    explicit NoexceptMoveString(const char* p) : s(p) {}
    NoexceptMoveString(const NoexceptMoveString&) = default;
    NoexceptMoveString(NoexceptMoveString&&) noexcept = default;
    NoexceptMoveString& operator=(const NoexceptMoveString&) = default;
    NoexceptMoveString& operator=(NoexceptMoveString&&) noexcept = default;
};

// Eyni, amma move constructor-da noexcept UNUDULUB.
struct ThrowingMoveString {
    std::string s;
    explicit ThrowingMoveString(const char* p) : s(p) {}
    ThrowingMoveString(const ThrowingMoveString&) = default;
    ThrowingMoveString(ThrowingMoveString&& o) : s(std::move(o.s)) {}  // noexcept YOXDUR
    ThrowingMoveString& operator=(const ThrowingMoveString&) = default;
    ThrowingMoveString& operator=(ThrowingMoveString&&) = default;
};

static_assert(std::is_nothrow_move_constructible_v<NoexceptMoveString>);
static_assert(!std::is_nothrow_move_constructible_v<ThrowingMoveString>);

template <typename V>
void push_n(std::size_t n) {
    V v;
    for (std::size_t i = 0; i < n; ++i) v.push_back(static_cast<int>(i));
    g_sink += v.size() + static_cast<std::uint64_t>(v[n / 2]);
}

template <typename V>
void push_n_reserved(std::size_t n) {
    V v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) v.push_back(static_cast<int>(i));
    g_sink += v.size() + static_cast<std::uint64_t>(v[n / 2]);
}

// ===========================================================================
// Sual 1: push_back həqiqətən amortized O(1)-dirmi?
// ===========================================================================

template <typename Growth>
std::uint64_t count_relocations(std::size_t n) {
    Counted::moves = 0;
    Counted::copies = 0;
    mystl::vector<Counted, Growth> v;
    for (std::size_t i = 0; i < n; ++i) v.emplace_back(static_cast<int>(i));  // özü move etmir
    return Counted::moves + Counted::copies;  // yalnız böyümə zamanı köçürmələr
}

std::uint64_t count_relocations_std(std::size_t n) {
    Counted::moves = 0;
    Counted::copies = 0;
    std::vector<Counted> v;
    for (std::size_t i = 0; i < n; ++i) v.emplace_back(static_cast<int>(i));
    return Counted::moves + Counted::copies;
}

void q1() {
    std::cout << "Q1: push_back amortized O(1)\n";
    Csv time("q1_push_back_time.csv", "impl,n,ns_per_op");
    for (std::size_t n : log_sizes(1'000, 10'000'000)) {
        time.row("mystl", n, median_ns([&] { push_n<Mine>(n); }) / static_cast<double>(n));
        time.row("std", n, median_ns([&] { push_n<Std>(n); }) / static_cast<double>(n));
    }
    Csv reloc("q1_relocations.csv", "impl,n,relocations,relocations_per_element");
    for (std::size_t n : log_sizes(1'000, 10'000'000)) {
        const auto r = count_relocations<mystl::growth::doubling>(n);
        reloc.row("mystl", n, r, static_cast<double>(r) / static_cast<double>(n));
        const auto s = count_relocations_std(n);
        reloc.row("std", n, s, static_cast<double>(s) / static_cast<double>(n));
    }
}

// ===========================================================================
// Sual 2: growth factor 2× və 1.5×
// ===========================================================================

template <typename Growth>
std::size_t count_reallocations(std::size_t n, std::size_t& final_capacity) {
    mystl::vector<int, Growth> v;
    std::size_t reallocations = 0;
    std::size_t capacity = 0;
    for (std::size_t i = 0; i < n; ++i) {
        v.push_back(static_cast<int>(i));
        if (v.capacity() != capacity) {
            capacity = v.capacity();
            ++reallocations;
        }
    }
    final_capacity = capacity;
    return reallocations;
}

template <typename Growth>
void q2_policy(const char* name, Csv& csv) {
    for (std::size_t n : log_sizes(1'000, 10'000'000)) {
        const double ns = median_ns([&] { push_n<mystl::vector<int, Growth>>(n); });
        std::size_t capacity = 0;
        const auto reallocations = count_reallocations<Growth>(n, capacity);
        const auto relocations = count_relocations<Growth>(n);
        csv.row(name, n, ns / static_cast<double>(n), reallocations, relocations, capacity,
                capacity - n, static_cast<double>(capacity - n) / static_cast<double>(capacity));
    }
}

// Boş yer (capacity - size) / capacity: hər ölçü üçün (1 .. limit), sıx nümunə.
template <typename Growth>
void q2_waste_curve(const char* name, Csv& curve, Csv& summary) {
    const std::size_t limit = bench::g_quick ? 100'000 : 1'000'000;
    mystl::vector<char, Growth> v;
    double waste_sum = 0;
    double waste_max = 0;
    std::size_t next_sample = 1;
    for (std::size_t size = 1; size <= limit; ++size) {
        v.push_back('x');
        const double waste =
            static_cast<double>(v.capacity() - size) / static_cast<double>(v.capacity());
        waste_sum += waste;
        if (size >= 16) waste_max = std::max(waste_max, waste);
        if (size >= next_sample) {
            curve.row(name, size, v.capacity(), waste);
            next_sample = std::max(next_sample + 1, static_cast<std::size_t>(next_sample * 1.01));
        }
    }
    summary.row(name, limit, waste_sum / static_cast<double>(limit), waste_max);
}

void q2() {
    std::cout << "Q2: growth factor 2x vs 1.5x\n";
    Csv csv("q2_growth.csv",
            "policy,n,ns_per_op,reallocations,relocations,capacity,waste,waste_ratio");
    q2_policy<mystl::growth::doubling>("2x", csv);
    q2_policy<mystl::growth::one_and_half>("1.5x", csv);

    Csv curve("q2_waste_curve.csv", "policy,size,capacity,waste_ratio");
    Csv summary("q2_waste_summary.csv", "policy,up_to_size,mean_waste_ratio,max_waste_ratio");
    q2_waste_curve<mystl::growth::doubling>("2x", curve, summary);
    q2_waste_curve<mystl::growth::one_and_half>("1.5x", curve, summary);
}

// ===========================================================================
// Sual 3: reserve nə qədər qazandırır?
// ===========================================================================

void q3() {
    std::cout << "Q3: reserve\n";
    Csv csv("q3_reserve.csv", "impl,variant,n,ns_per_op");
    for (std::size_t n : log_sizes(1'000, 10'000'000)) {
        const auto d = static_cast<double>(n);
        csv.row("mystl", "no_reserve", n, median_ns([&] { push_n<Mine>(n); }) / d);
        csv.row("mystl", "reserve", n, median_ns([&] { push_n_reserved<Mine>(n); }) / d);
        csv.row("std", "no_reserve", n, median_ns([&] { push_n<Std>(n); }) / d);
        csv.row("std", "reserve", n, median_ns([&] { push_n_reserved<Std>(n); }) / d);
    }
}

// ===========================================================================
// Sual 4: copy ilə move; noexcept unudulanda
// ===========================================================================

template <typename V>
void emplace_strings(std::size_t n) {
    V v;
    for (std::size_t i = 0; i < n; ++i) v.emplace_back(kText);
    g_sink += v.size() + v[n / 2].s.size();
}

template <typename V>
void q4_push_mode(const char* impl, std::size_t n, Csv& csv) {
    using Source = std::vector<std::string>;
    auto setup = [n] { return Source(n, std::string(kText)); };

    const double copy_ns = median_ns_with_setup(setup, [n](Source& src) {
        V v;
        v.reserve(n);
        for (auto& s : src) v.push_back(s);              // kopya: yeni heap yaddaşı
        g_sink += v.size() + v[n / 2].size();
    });
    const double move_ns = median_ns_with_setup(setup, [n](Source& src) {
        V v;
        v.reserve(n);
        for (auto& s : src) v.push_back(std::move(s));   // köçürmə: pointer götürülür
        g_sink += v.size() + v[n / 2].size();
    });
    const auto d = static_cast<double>(n);
    csv.row(impl, "push_back(copy)", n, copy_ns / d);
    csv.row(impl, "push_back(move)", n, move_ns / d);
}

void q4() {
    std::cout << "Q4: copy vs move, noexcept\n";
    Csv growth("q4_growth_noexcept.csv", "impl,element,n,ns_per_op");
    for (std::size_t n : log_sizes(1'000, 1'000'000)) {
        const auto d = static_cast<double>(n);
        growth.row("mystl", "noexcept_move", n,
                   median_ns([&] { emplace_strings<mystl::vector<NoexceptMoveString>>(n); }) / d);
        growth.row("mystl", "throwing_move", n,
                   median_ns([&] { emplace_strings<mystl::vector<ThrowingMoveString>>(n); }) / d);
        growth.row("std", "noexcept_move", n,
                   median_ns([&] { emplace_strings<std::vector<NoexceptMoveString>>(n); }) / d);
        growth.row("std", "throwing_move", n,
                   median_ns([&] { emplace_strings<std::vector<ThrowingMoveString>>(n); }) / d);
    }

    Csv push("q4_push_copy_vs_move.csv", "impl,mode,n,ns_per_op");
    for (std::size_t n : log_sizes(1'000, 1'000'000)) {
        q4_push_mode<mystl::vector<std::string>>("mystl", n, push);
        q4_push_mode<std::vector<std::string>>("std", n, push);
    }
}

// ===========================================================================
// Sual 5: ortaya insert/erase
// ===========================================================================

enum class Where { Front, Middle, Back };

std::size_t position_of(Where where, std::size_t size) {
    switch (where) {
        case Where::Front: return 0;
        case Where::Middle: return size / 2;
        case Where::Back: return size;
    }
    return 0;
}

template <typename V>
void q5_impl(const char* impl, Csv& csv) {
    const std::pair<Where, const char*> places[] = {
        {Where::Front, "front"}, {Where::Middle, "middle"}, {Where::Back, "back"}};
    for (std::size_t n : log_sizes(1'000, 10'000'000)) {
        const std::size_t ops = n >= 1'000'000 ? 20 : 200;
        for (const auto& [where, name] : places) {
            auto setup = [n, ops] {
                V v;
                v.reserve(n + ops);   // insert zamanı yenidən ayırma olmasın: yalnız sürüşdürmə ölçülür
                for (std::size_t i = 0; i < n; ++i) v.push_back(static_cast<int>(i));
                return v;
            };
            const double insert_ns = median_ns_with_setup(setup, [&](V& v) {
                for (std::size_t k = 0; k < ops; ++k) {
                    const auto pos = static_cast<std::ptrdiff_t>(position_of(where, v.size()));
                    v.insert(v.begin() + pos, static_cast<int>(k));
                }
                g_sink += v.size();
            });
            const double erase_ns = median_ns_with_setup(setup, [&](V& v) {
                for (std::size_t k = 0; k < ops; ++k) {
                    const std::size_t p = position_of(where, v.size());
                    const auto pos = static_cast<std::ptrdiff_t>(p == v.size() ? p - 1 : p);
                    v.erase(v.begin() + pos);
                }
                g_sink += v.size();
            });
            const auto d = static_cast<double>(ops);
            csv.row(impl, "insert", name, n, insert_ns / d);
            csv.row(impl, "erase", name, n, erase_ns / d);
        }
    }
}

void q5() {
    std::cout << "Q5: insert/erase by position\n";
    Csv csv("q5_insert_erase.csv", "impl,op,position,n,ns_per_op");
    q5_impl<Mine>("mystl", csv);
    q5_impl<Std>("std", csv);
}

// ===========================================================================
// Sual 6: cache-in təsiri — vector və sadə linked list-də traversal
// ===========================================================================

struct Node {
    int value;
    Node* next;
};

// Sadə tək əlaqəli siyahı. Düyünlər ayrı-ayrı new ilə ayrılır.
// shuffled = true: düyünlər yaddaşda təsadüfi sırada birləşdirilir
// (uzun müddət işləmiş proqramın "dağınıq" heap-ini təqlid edir).
class SimpleList {
public:
    SimpleList(std::size_t n, bool shuffled) {
        nodes_.reserve(n);
        for (std::size_t i = 0; i < n; ++i) nodes_.push_back(new Node{static_cast<int>(i), nullptr});
        std::vector<std::size_t> order(n);
        std::iota(order.begin(), order.end(), std::size_t{0});
        if (shuffled) {
            std::mt19937 rng(42);
            std::shuffle(order.begin(), order.end(), rng);
        }
        for (std::size_t i = 0; i + 1 < n; ++i) nodes_[order[i]]->next = nodes_[order[i + 1]];
        head_ = n ? nodes_[order[0]] : nullptr;
    }
    ~SimpleList() {
        for (Node* p : nodes_) delete p;
    }
    SimpleList(const SimpleList&) = delete;
    SimpleList& operator=(const SimpleList&) = delete;

    std::uint64_t sum() const {
        std::uint64_t s = 0;
        for (const Node* p = head_; p; p = p->next) s += static_cast<std::uint64_t>(p->value);
        return s;
    }

private:
    std::vector<Node*> nodes_;
    Node* head_ = nullptr;
};

template <typename V>
std::uint64_t sum_of(const V& v) {
    std::uint64_t s = 0;
    for (int x : v) s += static_cast<std::uint64_t>(x);
    return s;
}

void q6() {
    std::cout << "Q6: traversal, vector vs linked list\n";
    Csv csv("q6_traversal.csv", "container,n,ns_per_element");
    for (std::size_t n : log_sizes(1'000, 10'000'000)) {
        const auto d = static_cast<double>(n);
        {
            Mine v;
            for (std::size_t i = 0; i < n; ++i) v.push_back(static_cast<int>(i));
            csv.row("mystl::vector", n, median_ns([&] { g_sink += sum_of(v); }) / d);
        }
        {
            Std v(n);
            std::iota(v.begin(), v.end(), 0);
            csv.row("std::vector", n, median_ns([&] { g_sink += sum_of(v); }) / d);
        }
        {
            const SimpleList list(n, false);
            csv.row("list (sequential nodes)", n, median_ns([&] { g_sink += list.sum(); }) / d);
        }
        {
            const SimpleList list(n, true);
            csv.row("list (shuffled nodes)", n, median_ns([&] { g_sink += list.sum(); }) / d);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> selected;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--quick") bench::g_quick = true;
        else if (arg.rfind("--repeats=", 0) == 0) bench::g_repeats = std::stoi(arg.substr(10));
        else if (arg.rfind("--out=", 0) == 0) bench::g_out_dir = arg.substr(6);
        else selected.push_back(arg);
    }
    auto wanted = [&](const char* q) {
        return selected.empty() || std::find(selected.begin(), selected.end(), q) != selected.end();
    };

    std::cout << "repeats = " << bench::g_repeats << (bench::g_quick ? ", QUICK mode" : "") << '\n';
    if (wanted("q1")) q1();
    if (wanted("q2")) q2();
    if (wanted("q3")) q3();
    if (wanted("q4")) q4();
    if (wanted("q5")) q5();
    if (wanted("q6")) q6();
    // Sual 7 (std::vector-a nə qədər yaxın) ayrıca ölçülmür: 1–6-cı sualların
    // hər birində std::vector müqayisə xətti kimi ölçülür.

    std::cout << "sink = " << g_sink << '\n';   // nəticələr "istifadə olunur"
}
