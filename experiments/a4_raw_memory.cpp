// A4: new T[] modeli ilə [qurulmuş | xam] yaddaş modelinin müqayisəsi.
//
// OldVector, v3-ün (A3) sadələşdirilmiş surətidir: yaddaşı new T[] ilə alır.
// mystl::vector isə v4-dür: yaddaşı ::operator new ilə alır, obyektləri
// yalnız lazım olanda qurur (placement new / std::construct_at).
//
// Eyni əməliyyatları ikisində də edib Noisy-nin çıxışını müqayisə edirik.

#include <mystl/vector.hpp>

#include <cstddef>
#include <iostream>

class Noisy {
public:
    Noisy() : id_(++counter_) { std::cout << "      ctor    #" << id_ << '\n'; }
    Noisy(const Noisy& other) : id_(++counter_) {
        std::cout << "      copy    #" << id_ << " <- #" << other.id_ << '\n';
    }
    Noisy& operator=(const Noisy& other) {
        std::cout << "      assign  #" << id_ << " <- #" << other.id_ << '\n';
        return *this;
    }
    ~Noisy() { std::cout << "      dtor    #" << id_ << '\n'; }

private:
    inline static int counter_ = 0;
    int id_;
};

// v3 modeli: new T[] / delete[]
template <typename T>
class OldVector {
public:
    ~OldVector() { delete[] data_; }

    void reserve(std::size_t n) {
        if (n <= capacity_) return;
        T* new_data = new T[n];
        for (std::size_t i = 0; i < size_; ++i) new_data[i] = data_[i];
        delete[] data_;
        data_ = new_data;
        capacity_ = n;
    }
    void push_back(const T& value) {
        if (size_ == capacity_) reserve(capacity_ == 0 ? 1 : capacity_ * 2);
        data_[size_++] = value;
    }
    void pop_back() { --size_; }  // obyektlə heç nə etmək mümkün deyil

private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

template <typename Vec>
void run(const char* title) {
    std::cout << "=== " << title << " ===\n";
    Noisy item;  // push_back üçün mənbə obyekt
    {
        Vec v;
        std::cout << "  1) reserve(3) on an empty vector:\n";
        v.reserve(3);

        std::cout << "  2) push_back(item):\n";
        v.push_back(item);

        std::cout << "  3) pop_back():\n";
        v.pop_back();

        std::cout << "  4) end of scope (vector destroyed):\n";
    }
    std::cout << "  (source item destroyed:)\n";
}

int main() {
    run<OldVector<Noisy>>("OldVector: new T[] model (v3)");
    std::cout << '\n';
    run<mystl::vector<Noisy>>("mystl::vector: raw memory model (v4)");
}
