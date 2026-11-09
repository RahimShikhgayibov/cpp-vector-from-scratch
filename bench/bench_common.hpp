#pragma once

// Benchmark üçün ortaq köməkçilər: vaxt ölçmə, n dəyərləri, CSV yazma.
//
// Metodologiya (yol xəritəsi, Faza D):
//   - yalnız Release build (-O2), sanitizer-siz
//   - std::chrono::steady_clock
//   - hər ölçüm əvvəlcə bir dəfə "ısınma" üçün işlədilir, sonra ən azı 5 dəfə
//     təkrarlanır və MEDIAN götürülür (təsadüfi gecikmələrə davamlıdır)
//   - kompilyator nəticəsi istifadə olunmayan kodu silə bilər: nəticələr
//     g_sink-ə yığılır və proqramın sonunda çap olunur

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bench {

inline std::uint64_t g_sink = 0;   // optimizatorun kodu silməməsi üçün
inline int g_repeats = 5;
inline bool g_quick = false;       // --quick: kiçik n, tez yoxlama üçün
inline std::filesystem::path g_out_dir = "docs/benchmark/data";

using Clock = std::chrono::steady_clock;

inline double elapsed_ns(Clock::time_point start, Clock::time_point stop) {
    return std::chrono::duration<double, std::nano>(stop - start).count();
}

inline double median(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

// body()-ni ölçür: 1 ısınma + g_repeats ölçüm, median nanosaniyə.
template <typename Body>
double median_ns(Body&& body) {
    body();
    std::vector<double> samples;
    for (int i = 0; i < g_repeats; ++i) {
        const auto start = Clock::now();
        body();
        samples.push_back(elapsed_ns(start, Clock::now()));
    }
    return median(std::move(samples));
}

// Hər ölçümdən əvvəl ölçülməyən hazırlıq lazım olanda:
// setup() -> state, sonra yalnız body(state) ölçülür.
template <typename Setup, typename Body>
double median_ns_with_setup(Setup&& setup, Body&& body) {
    {
        auto state = setup();
        body(state);
    }
    std::vector<double> samples;
    for (int i = 0; i < g_repeats; ++i) {
        auto state = setup();
        const auto start = Clock::now();
        body(state);
        samples.push_back(elapsed_ns(start, Clock::now()));
    }
    return median(std::move(samples));
}

// 10^lo .. 10^hi aralığında loqarifmik addımlar: 1, 2, 5, 10, 20, 50, ...
inline std::vector<std::size_t> log_sizes(std::size_t lo, std::size_t hi) {
    if (g_quick) hi = std::min<std::size_t>(hi, 100'000);
    std::vector<std::size_t> result;
    for (std::size_t decade = 1; decade <= hi; decade *= 10) {
        for (std::size_t m : {1, 2, 5}) {
            const std::size_t n = decade * m;
            if (n >= lo && n <= hi) result.push_back(n);
        }
    }
    return result;
}

// Sadə CSV yazıcı.
class Csv {
public:
    Csv(const std::string& name, const std::string& header) {
        std::filesystem::create_directories(g_out_dir);
        const auto path = g_out_dir / name;
        file_.open(path);
        if (!file_) throw std::runtime_error("cannot open " + path.string());
        file_ << header << '\n';
        std::cout << "  -> " << path.string() << '\n';
    }

    template <typename... Fields>
    void row(const Fields&... fields) {
        bool first = true;
        ((file_ << (first ? "" : ",") << fields, first = false), ...);
        file_ << '\n';
        file_.flush();
    }

private:
    std::ofstream file_;
};

}  // namespace bench
