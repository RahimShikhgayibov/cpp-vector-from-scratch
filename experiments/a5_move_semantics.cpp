// A5: Move semantics. noexcept unudulanda nə baş verir?
//
// İki tip var, yeganə fərqləri move constructor-un noexcept olub-olmamasıdır.
// Hər birindən 2 elementli dolu vektora 3-cü elementi əlavə edirik (böyümə baş verir)
// və çıxışda köhnə elementlərin KÖÇÜRÜLDÜYÜNÜ və ya KOPYALANDIĞINI görürük.

#include <mystl/vector.hpp>

#include <iostream>
#include <utility>

template <bool NoexceptMove>
class Noisy {
public:
    explicit Noisy(int id) : id_(id) { std::cout << "      ctor  #" << id_ << '\n'; }
    Noisy(const Noisy& other) : id_(other.id_) { std::cout << "      COPY  #" << id_ << '\n'; }
    Noisy(Noisy&& other) noexcept(NoexceptMove) : id_(other.id_) {
        std::cout << "      move  #" << id_ << '\n';
    }
    ~Noisy() = default;

private:
    int id_;
};

template <bool NoexceptMove>
void grow_demo(const char* title) {
    std::cout << "=== " << title << " ===\n";
    mystl::vector<Noisy<NoexceptMove>> v;
    v.reserve(2);
    std::cout << "  push_back(Noisy(1)), push_back(Noisy(2)):\n";
    v.push_back(Noisy<NoexceptMove>(1));
    v.push_back(Noisy<NoexceptMove>(2));
    std::cout << "  push_back(Noisy(3))  <- full, vector grows:\n";
    v.push_back(Noisy<NoexceptMove>(3));
    std::cout << '\n';
}

int main() {
    grow_demo<true>("move ctor IS noexcept");
    grow_demo<false>("move ctor is NOT noexcept");

    std::cout << "=== moving a whole vector ===\n";
    mystl::vector<Noisy<true>> a;
    a.reserve(3);
    a.push_back(Noisy<true>(10));
    a.push_back(Noisy<true>(11));
    std::cout << "  mystl::vector b = std::move(a):\n";
    mystl::vector<Noisy<true>> b = std::move(a);
    std::cout << "  (nothing printed: only 3 pointers/sizes changed hands)\n";
    std::cout << "  a.size() = " << a.size() << ", b.size() = " << b.size() << '\n';
}
