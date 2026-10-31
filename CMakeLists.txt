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

option(MINIM_BUILD_TESTS "Build the tests" OFF)
option(MINIM_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MINIM_GC_STRESS "Collect garbage frequently, every MINIM_GC_STRESS_INTERVAL allocations" OFF)
set(MINIM_GC_STRESS_INTERVAL 1 CACHE STRING "Allocations between collections under MINIM_GC_STRESS")

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
    if(NOT MINIM_GC_STRESS_INTERVAL MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "MINIM_GC_STRESS_INTERVAL must be a positive integer, not '${MINIM_GC_STRESS_INTERVAL}'")
    endif()
    add_compile_definitions(MINIM_GC_STRESS=${MINIM_GC_STRESS_INTERVAL})
endif()

# The prelude's source, embedded as a C array
set(prelude_c ${CMAKE_CURRENT_BINARY_DIR}/prelude_scm.c)
add_custom_command(
    OUTPUT ${prelude_c}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/src/prelude.scm
        -DOUTPUT=${prelude_c}
        -DNAME=minim_prelude_scm
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake
    DEPENDS src/prelude.scm cmake/embed.cmake
    COMMENT "Embedding prelude.scm"
)

add_library(libminim STATIC
    ${prelude_c}
    src/char.c
    src/env.c
    src/error.c
    src/eval.c
    src/gc.c
    src/heap.c
    src/object.c
    src/prelude.c
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
