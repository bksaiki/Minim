cmake_minimum_required(VERSION 3.21)
project(minim LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MINIM_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MINIM_GC_STRESS "Collect garbage on every allocation" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
    if(MINIM_SANITIZE)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

if(MINIM_GC_STRESS)
    add_compile_definitions(MINIM_GC_STRESS)
endif()

# Header-only until it has sources.
add_library(minim_core INTERFACE)
target_include_directories(minim_core INTERFACE include)

add_executable(minim src/main.c)
target_link_libraries(minim PRIVATE minim_core)

enable_testing()
add_subdirectory(tests)
