cmake_minimum_required(VERSION 3.21)
project(minim LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Without a build type, single-config generators build unoptimized.
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MINIM_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(MINIM_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MINIM_GC_STRESS "Collect garbage on every allocation" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(MINIM_SANITIZE)
        # Without -fno-sanitize-recover, UBSan reports errors but exits 0.
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
                            -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
endif()

if(MINIM_GC_STRESS)
    add_compile_definitions(MINIM_GC_STRESS)
endif()

add_library(libminim STATIC
    src/char.c
    src/env.c
    src/error.c
    src/eval.c
    src/gc.c
    src/heap.c
    src/object.c
    src/prims.c
    src/print.c
    src/read.c
    src/symbol.c
)
target_include_directories(libminim PUBLIC include PRIVATE src)
set_target_properties(libminim PROPERTIES OUTPUT_NAME minim)  # libminim.a, not liblibminim.a

add_executable(minim src/main.c)
target_link_libraries(minim PRIVATE libminim)

if(MINIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
