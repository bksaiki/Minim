cmake_minimum_required(VERSION 3.21)
project(minim VERSION 0.1.0 LANGUAGES C)

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

# The Scheme sources, in load order, and the boot file they expand into.
set(MINIM_SCHEME_SOURCES lists control records expander libraries derived standard)
list(TRANSFORM MINIM_SCHEME_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/scheme/)
list(TRANSFORM MINIM_SCHEME_SOURCES APPEND .scm)
set(MINIM_BOOT ${CMAKE_CURRENT_SOURCE_DIR}/scheme/prelude.boot)

# The boot file, embedded as a C array
set(prelude_c ${CMAKE_CURRENT_BINARY_DIR}/prelude_boot.c)
add_custom_command(
    OUTPUT ${prelude_c}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${MINIM_BOOT}
        -DOUTPUT=${prelude_c}
        -DNAME=minim_prelude_boot
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake
    DEPENDS ${MINIM_BOOT} cmake/embed.cmake
    COMMENT "Embedding prelude.boot"
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
    src/prims/char.c
    src/prims/control.c
    src/prims/equal.c
    src/prims/error.c
    src/prims/number.c
    src/prims/pair.c
    src/prims/prims.c
    src/prims/record.c
    src/prims/string.c
    src/prims/syntax.c
    src/prims/system.c
    src/prims/vector.c
    src/print.c
    src/read.c
    src/stack.c
    src/symbol.c
    src/system.c
)
target_include_directories(libminim PUBLIC include PRIVATE src)
set_target_properties(libminim PROPERTIES OUTPUT_NAME minim)  # libminim.a, not liblibminim.a

add_executable(minim src/main.c)
target_link_libraries(minim PRIVATE libminim)
target_include_directories(minim PRIVATE src)
target_compile_definitions(minim PRIVATE MINIM_VERSION="${PROJECT_VERSION}")

# Writes the Scheme sources expanded into core forms, for the boot file.
add_executable(boot_gen tools/boot_gen.c)
target_link_libraries(boot_gen PRIVATE libminim)
target_include_directories(boot_gen PRIVATE src)

# Regenerates the boot file from the Scheme sources, replacing it only if
# generation succeeds.  After an expander change, run it twice: the second run
# expands with the new expander.
add_custom_target(boot
    COMMAND boot_gen ${CMAKE_CURRENT_SOURCE_DIR}/tools/boot.scm
        ${CMAKE_CURRENT_BINARY_DIR}/prelude.boot ${MINIM_SCHEME_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/prelude.boot ${MINIM_BOOT}
    COMMENT "Regenerating scheme/prelude.boot"
)

if(MINIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
