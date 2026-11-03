# minim_add_stressed(name command...): adds the test `name`, and `name_stress`,
# the same test labelled `stress` and run with a collection at every
# allocation, startup included, on the default schedule.
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

# The boot file must be what boot_gen makes of the Scheme sources, which also
# means it reproduces itself, since boot_gen starts from it.
string(REPLACE ";" "|" scheme_sources "${MINIM_SCHEME_SOURCES}")
add_test(NAME boot COMMAND ${CMAKE_COMMAND}
    -DBOOT_GEN=$<TARGET_FILE:boot_gen>
    -DBOOT_SCM=${PROJECT_SOURCE_DIR}/tools/boot.scm
    -DSOURCES=${scheme_sources}
    -DEXPECTED=${MINIM_BOOT}
    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/prelude.boot
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_boot.cmake)

minim_add_test(tag)
minim_add_test(object)
minim_add_test(heap INTERNAL)
minim_add_test(gc INTERNAL)
minim_add_test(stack INTERNAL)
minim_add_test(string INTERNAL)
minim_add_test(symbol)
minim_add_test(print INTERNAL)
minim_add_test(error)
minim_add_test(read INTERNAL)
minim_add_test(eval INTERNAL)
minim_add_test(prims)
minim_add_test(port)

minim_test_executable(startup INTERNAL)
add_test(NAME startup COMMAND test_startup)
set_tests_properties(startup PROPERTIES ENVIRONMENT "MINIM_GC_NURSERY_BYTES=1")

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

# minim_add_script_test(name [NO_STRESS] [STATUS n] [INPUT file] [ARGS arg...]
#                       [ENV var=value...]):
# runs `minim ARGS` (default `<name>.scm`) from scripts/ with ENV set and
# scripts/INPUT, if given, on stdin, and checks the exit status (default 0),
# stdout against scripts/<name>.out, and stderr against scripts/<name>.err if
# it exists.  A test with ENV or NO_STRESS has no stress copy.
function(minim_add_script_test name)
    cmake_parse_arguments(PARSE_ARGV 1 opt "NO_STRESS" "STATUS;INPUT" "ARGS;ENV")
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
        $<$<BOOL:${opt_INPUT}>:-DINPUT=${dir}/${opt_INPUT}>
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

minim_add_repl_test(aliases)
minim_add_repl_test(bodies)
minim_add_repl_test(boot)
minim_add_repl_test(chars)
minim_add_repl_test(continuations)
minim_add_repl_test(core)
minim_add_repl_test(derived)
minim_add_repl_test(equality)
minim_add_repl_test(eval)
minim_add_repl_test(exceptions)
minim_add_repl_test(expansion)
minim_add_repl_test(files)
minim_add_repl_test(gc)
minim_add_repl_test(guard)
minim_add_repl_test(hashing)
minim_add_repl_test(libraries)
minim_add_repl_test(lists)
minim_add_repl_test(load)
minim_add_repl_test(local-macros)
minim_add_repl_test(numbers)
minim_add_repl_test(parameters)
minim_add_repl_test(ports)
minim_add_repl_test(quasiquote)
minim_add_repl_test(records)
minim_add_repl_test(strings)
minim_add_repl_test(symbols)
minim_add_repl_test(syntax-rules)
minim_add_repl_test(top-level)
minim_add_repl_test(values)
minim_add_repl_test(vectors)

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
minim_add_script_test(command-line ARGS command-line.scm a "b c")
minim_add_script_test(command-line-option ARGS command-line.scm --x --help)
minim_add_script_test(command-line-program ARGS --program command-line-program.scm a --x)
minim_add_script_test(environment-variable
    ENV "MINIM_TEST_VARIABLE=set by the test")
minim_add_script_test(hello)
minim_add_script_test(library-path ENV MINIM_LIBRARY_PATH=nowhere:libpath)
minim_add_script_test(missing STATUS 1)
minim_add_script_test(program ARGS --program program.scm)
minim_add_script_test(program-eval ARGS --program program-eval.scm)
minim_add_script_test(program-no-import STATUS 1 ARGS --program program-no-import.scm)
minim_add_script_test(program-own ARGS --program program-own.scm)
minim_add_script_test(program-unbound STATUS 1 ARGS --program program-unbound.scm)
minim_add_script_test(read-error STATUS 1)
minim_add_script_test(stdin INPUT stdin.txt)
minim_add_script_test(file-ports)
minim_add_script_test(help ARGS --help)
minim_add_script_test(version ARGS --version)
minim_add_script_test(usage-option STATUS 2 ARGS --foo)
minim_add_script_test(usage-program STATUS 2 ARGS --program)

# The R7RS test suite's sections, read from its test-begin lines
# (r7rs/r7rs-tests.scm): each runs with
# r7rs/run.scm, labelled r7rs, its report checked against r7rs/<slug>.out, the
# slug its number or else its title in lowercase and hyphenated.  The target
# r7rs-golden rewrites the reports.  No stress copies: the suite is slow under
# stress, and the other tests stress the same code.
set(r7rs_dir ${CMAKE_CURRENT_SOURCE_DIR}/r7rs)
set(script ${CMAKE_CURRENT_SOURCE_DIR}/run_minim.cmake)
set(r7rs_golden)
file(STRINGS ${r7rs_dir}/r7rs-tests.scm r7rs_begins REGEX "^\\(test-begin \"")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${r7rs_dir}/r7rs-tests.scm)
foreach(begin IN LISTS r7rs_begins)
    string(REGEX REPLACE "^\\(test-begin \"(.*)\"\\)$" "\\1" title "${begin}")
    if(title STREQUAL "R7RS")  # the whole suite, around the sections
        continue()
    endif()
    string(REGEX MATCH "^[0-9.]+" slug "${title}")
    if(NOT slug)
        string(TOLOWER "${title}" slug)
        string(REPLACE " " "-" slug "${slug}")
    endif()
    set(args -DMINIM=$<TARGET_FILE:minim> -DDIR=${r7rs_dir} "-DARGS=run.scm|${title}"
        -DEXPECTED=${r7rs_dir}/${slug}.out)
    add_test(NAME r7rs_${slug} COMMAND ${CMAKE_COMMAND} ${args} -P ${script})
    set_tests_properties(r7rs_${slug} PROPERTIES LABELS r7rs)
    list(APPEND r7rs_golden COMMAND ${CMAKE_COMMAND} ${args} -DUPDATE=ON -P ${script})
endforeach()
add_custom_target(r7rs-golden ${r7rs_golden} DEPENDS minim VERBATIM
    COMMENT "Rewriting the R7RS test suite's reports")
