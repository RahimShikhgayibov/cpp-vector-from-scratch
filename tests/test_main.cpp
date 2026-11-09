// doctest-in main() funksiyası yalnız BİR .cpp faylında yaradılmalıdır.
// Digər test faylları sadəcə #include "doctest.h" edir.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

TEST_CASE("test framework works") {
    CHECK(1 + 1 == 2);
}
