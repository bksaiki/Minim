# minim_add_stressed(name command...): adds the test `name`, and `name_stress`,
# the same test labelled `stress` and run, after startup, with a collection at
# every allocation on the default schedule.
function(minim_add_stressed name)
    add_test(NAME ${name} COMMAND ${ARGN})
    add_test(NAME ${name}_stress COMMAND ${ARGN})
    set_tests_properties(${name}_stress PROPERTIES
        LABELS stress
        ENVIRONMENT "MINIM_GC_NURSERY_BYTES=1")
endfunction()

# minim_test_executable(name [INTERNAL]): builds test_<name> from test_<name>.c;
# INTERNAL tests may include headers from src/.
function(minim_test_executable name)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE libminim)
    if("INTERNAL" IN_LIST ARGN)
        target_include_directories(test_${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    endif()
endfunction()

# minim_add_test(name [INTERNAL]): builds test_<name> and adds it, stressed too.
function(minim_add_test name)
    minim_test_executable(${name} ${ARGN})
    minim_add_stressed(${name} test_${name})
endfunction()

# src/prelude.boot must be what boot_gen makes of src/prelude.scm, which also
# means it reproduces itself, since boot_gen starts from it.
add_test(NAME boot COMMAND ${CMAKE_COMMAND}
    -DBOOT_GEN=$<TARGET_FILE:boot_gen>
    -DSRC=${PROJECT_SOURCE_DIR}/src
    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/prelude.boot
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_boot.cmake)

minim_add_test(object)
minim_add_test(heap INTERNAL)
minim_add_test(gc INTERNAL)
minim_add_test(string INTERNAL)
minim_add_test(symbol)
minim_add_test(print INTERNAL)
minim_add_test(error)
minim_add_test(read INTERNAL)
minim_add_test(procedure)
minim_add_test(eval INTERNAL)
minim_add_test(prims)

# Startup runs with the default collector settings unless MINIM_GC_STARTUP=1,
# so startup is stressed only here.
minim_test_executable(startup INTERNAL)
add_test(NAME startup COMMAND test_startup program)
set_tests_properties(startup PROPERTIES ENVIRONMENT "MINIM_GC_NURSERY_BYTES=1")
add_test(NAME startup_stress COMMAND test_startup startup)
set_tests_properties(startup_stress PROPERTIES
    LABELS stress
    ENVIRONMENT "MINIM_GC_STARTUP=1;MINIM_GC_NURSERY_BYTES=1")

# A stale pointer must stay poisoned; only meaningful under AddressSanitizer.
if(MINIM_SANITIZE)
    minim_test_executable(stale INTERNAL)
    add_test(NAME stale COMMAND test_stale)
    # ASan aborts on macOS by default, which CTest counts as a failure.
    set_tests_properties(stale PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=abort_on_error=0"
        PASS_REGULAR_EXPRESSION "AddressSanitizer: use-after-poison")
endif()

# minim_add_repl_test(name): pipes repl/<name>.scm into minim, run from repl/,
# and compares stdout with repl/<name>.out, and stderr with repl/<name>.err if
# it exists.
function(minim_add_repl_test name)
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/repl)
    minim_add_stressed(repl_${name}
        ${CMAKE_COMMAND}
            -DMINIM=$<TARGET_FILE:minim>
            -DDIR=${dir}
            -DINPUT=${dir}/${name}.scm
            -DEXPECTED=${dir}/${name}.out
            -DEXPECTED_ERR=${dir}/${name}.err
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_minim.cmake)
endfunction()

# minim_add_script_test(name [NO_STRESS] [STATUS n] [ARGS arg...]
#                       [ENV var=value...]):
# runs `minim ARGS` (default `<name>.scm`) from scripts/ with ENV set, and
# checks the exit status (default 0), stdout against scripts/<name>.out, and
# stderr against scripts/<name>.err if it exists.  A test with ENV or
# NO_STRESS has no stress copy.
function(minim_add_script_test name)
    cmake_parse_arguments(PARSE_ARGV 1 opt "NO_STRESS" "STATUS" "ARGS;ENV")
    if(NOT DEFINED opt_STATUS)
        set(opt_STATUS 0)
    endif()
    if(NOT opt_ARGS)
        set(opt_ARGS ${name}.scm)
    endif()
    string(REPLACE ";" "|" args "${opt_ARGS}")
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
    set(command ${CMAKE_COMMAND}
        -DMINIM=$<TARGET_FILE:minim>
        -DDIR=${dir}
        -DARGS=${args}
        -DSTATUS=${opt_STATUS}
        -DEXPECTED=${dir}/${name}.out
        -DEXPECTED_ERR=${dir}/${name}.err
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_minim.cmake)
    if(opt_ENV OR opt_NO_STRESS)
        add_test(NAME script_${name} COMMAND ${command})
        if(opt_ENV)
            set_tests_properties(script_${name} PROPERTIES ENVIRONMENT "${opt_ENV}")
        endif()
    else()
        minim_add_stressed(script_${name} ${command})
    endif()
endfunction()

minim_add_repl_test(boot)
minim_add_repl_test(chars)
minim_add_repl_test(derived)
minim_add_repl_test(echo)
minim_add_repl_test(equality)
minim_add_repl_test(eval)
minim_add_repl_test(expand)
minim_add_repl_test(expander)
minim_add_repl_test(expansion)
minim_add_repl_test(gc)
minim_add_repl_test(hashing)
minim_add_repl_test(lists)
minim_add_repl_test(load)
minim_add_repl_test(local-macros)
minim_add_repl_test(prelude)
minim_add_repl_test(prims)
minim_add_repl_test(strings)
minim_add_repl_test(symbols)
minim_add_repl_test(syntax-rules)
minim_add_repl_test(tail)
minim_add_repl_test(values)

minim_add_script_test(fail STATUS 1)
minim_add_script_test(gc-settings NO_STRESS)  # the defaults
minim_add_script_test(gc-settings-env ARGS gc-settings.scm
    ENV MINIM_GC_NURSERY_BYTES=1 MINIM_GC_RADIX=16 MINIM_GC_MAX_GENERATION=3)
minim_add_script_test(gc-settings-bad-nursery STATUS 1 ARGS gc-settings.scm
    ENV MINIM_GC_NURSERY_BYTES=0)
minim_add_script_test(gc-settings-bad-radix STATUS 1 ARGS gc-settings.scm ENV MINIM_GC_RADIX=0)
minim_add_script_test(gc-settings-bad-generation STATUS 1 ARGS gc-settings.scm
    ENV MINIM_GC_MAX_GENERATION=9)
minim_add_script_test(gc-settings-not-number STATUS 1 ARGS gc-settings.scm
    ENV MINIM_GC_NURSERY_BYTES=1k)
minim_add_script_test(gc-settings-big-nursery STATUS 1 ARGS gc-settings.scm
    ENV MINIM_GC_NURSERY_BYTES=1152921504606846976)
minim_add_script_test(gc-settings-bad-startup STATUS 1 ARGS gc-settings.scm ENV MINIM_GC_STARTUP=2)
minim_add_script_test(hello)
minim_add_script_test(missing STATUS 1)
minim_add_script_test(usage STATUS 2 ARGS a.scm b.scm)
