#!/usr/bin/env bash
# Faza C: bütün testləri Linux-da AddressSanitizer + UndefinedBehaviorSanitizer ilə işlədir.
#
# Windows-dan Docker ilə (layihənin kök qovluğunda):
#   docker run --rm --cap-add=SYS_PTRACE -v "${PWD}:/src" -w /src gcc:14 bash scripts/asan_linux.sh
# Linux / WSL-də birbaşa:
#   bash scripts/asan_linux.sh
#
# --cap-add=SYS_PTRACE: LeakSanitizer proqramın sonunda yaddaşı yoxlamaq üçün
# ptrace istifadə edir; Docker onu default olaraq qadağan edir.
#
# Bayraqlar:
#   -fsanitize=address,undefined   ASan (yaddaş) + UBSan (undefined behavior)
#   -fno-sanitize-recover=all      ilk xətada dayan (testlər "keçdi" deməsin)
#   -fno-omit-frame-pointer        xəta hesabatında dəqiq stack trace

set -u
OUT=/tmp/asan-build
mkdir -p "$OUT"
FLAGS="-std=c++20 -O0 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iinclude"
export ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1
export UBSAN_OPTIONS=print_stacktrace=1

echo "=== $(g++ --version | head -n 1)"
status=0

echo "=== unit tests"
g++ $FLAGS tests/test_*.cpp -o "$OUT/tests" || exit 1
"$OUT/tests" || status=1

echo "=== differential test (10 seeds x 100000 steps)"
g++ $FLAGS tests/differential.cpp -o "$OUT/differential" || exit 1
"$OUT/differential" || status=1

echo "=== experiments that must be clean (a4, a5)"
for e in a4_raw_memory a5_move_semantics; do
    g++ $FLAGS "experiments/$e.cpp" -o "$OUT/$e" || exit 1
    "$OUT/$e" > /dev/null && echo "$e: clean" || status=1
done

# A0 və A2 bilərəkdən səhvdir: ASan onları TUTMALIDIR.
echo "=== experiments with intentional bugs (ASan must report them)"
g++ $FLAGS experiments/a0_noisy.cpp -o "$OUT/a0_noisy" || exit 1
if "$OUT/a0_noisy" > /dev/null 2> "$OUT/a0.log"; then
    echo "a0_noisy: NOT detected (unexpected)"; status=1
else
    echo "a0_noisy (new without delete): $(grep -m1 -o 'LeakSanitizer: detected memory leaks' "$OUT/a0.log")"
fi
g++ $FLAGS experiments/a2_shallow_copy.cpp -o "$OUT/a2_shallow_copy" || exit 1
if "$OUT/a2_shallow_copy" > /dev/null 2> "$OUT/a2.log"; then
    echo "a2_shallow_copy: NOT detected (unexpected)"; status=1
else
    echo "a2_shallow_copy (shallow copy): $(grep -m1 -o 'AddressSanitizer: attempting double-free' "$OUT/a2.log")"
fi

if [ "$status" -eq 0 ]; then echo "=== RESULT: all sanitizer checks passed"; else echo "=== RESULT: FAILED"; fi
exit "$status"
