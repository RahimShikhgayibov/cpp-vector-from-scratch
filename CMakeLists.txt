cmake_minimum_required(VERSION 3.20)
project(mystl LANGUAGES CXX)

# --- Dil standartı ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # C++20 yoxdursa, səssizcə köhnəyə düşmə
set(CMAKE_CXX_EXTENSIONS OFF)        # -std=gnu++20 yox, təmiz -std=c++20

# Build tipi verilməyibsə, Debug götür
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build tipi" FORCE)
endif()

# --- Kitabxananın özü (header-only) -------------------------------------------
# INTERFACE kitabxana: kompilyasiya olunan .cpp faylı yoxdur, yalnız
# "məni istifadə edən hər kəs include/ qovluğunu görsün" qaydası daşıyır.
add_library(mystl INTERFACE)
target_include_directories(mystl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Warning-lər ---------------------------------------------------------------
add_library(mystl_warnings INTERFACE)
target_compile_options(mystl_warnings INTERFACE -Wall -Wextra -Wpedantic)

# --- Sanitizer-lər (yalnız Debug) ---------------------------------------------
option(MYSTL_SANITIZE "Debug build-də sanitizer-ləri qoş" ON)
add_library(mystl_sanitizers INTERFACE)
if(MYSTL_SANITIZE)
  if(MINGW)
    # Windows MinGW-də libasan/libubsan yoxdur. UBSan-ı "trap" rejimində
    # işlədirik: undefined behavior olanda proqram dərhal dayanır.
    # AddressSanitizer üçün Linux və ya WSL lazımdır (README-yə bax).
    target_compile_options(mystl_sanitizers INTERFACE
      $<$<CONFIG:Debug>:-fsanitize=undefined -fsanitize-trap=all>)
  else()
    target_compile_options(mystl_sanitizers INTERFACE
      $<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>)
    target_link_options(mystl_sanitizers INTERFACE
      $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
  endif()
endif()

# --- Testlər -------------------------------------------------------------------
enable_testing()
add_executable(tests
  tests/test_main.cpp
  tests/test_int_vector.cpp
)
target_link_libraries(tests PRIVATE mystl mystl_warnings mystl_sanitizers)
add_test(NAME tests COMMAND tests)

# --- Təcrübələr (Faza A-nın kiçik proqramları) -------------------------------
add_executable(a0_noisy experiments/a0_noisy.cpp)
target_link_libraries(a0_noisy PRIVATE mystl_warnings mystl_sanitizers)
