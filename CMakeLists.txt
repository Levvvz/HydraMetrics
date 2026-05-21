cmake_minimum_required(VERSION 3.22)
project(HydraMetrics VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX /O2)
else()
    # Для GCC / Clang:
    add_compile_options(-Wall -Wextra -Werror -Wpedantic)
    add_compile_options($<$<CONFIG:Release>:-O3 -march=native -flto>)
    add_link_options($<$<CONFIG:Release>:-flto>)
endif()

find_package(Boost REQUIRED COMPONENTS system)
find_package(Redis++ REQUIRED)
find_package(GTest REQUIRED)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(benchmarks)
