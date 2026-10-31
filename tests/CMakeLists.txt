# minim_add_test(name [INTERNAL]): INTERNAL tests may include headers from src/.
function(minim_add_test name)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE libminim)
    if("INTERNAL" IN_LIST ARGN)
        target_include_directories(test_${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

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

# minim_add_repl_test(name): pipes repl/<name>.scm into minim, run from repl/,
# and compares stdout with repl/<name>.out, and stderr with repl/<name>.err if
# it exists.
function(minim_add_repl_test name)
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/repl)
    add_test(NAME repl_${name}
        COMMAND ${CMAKE_COMMAND}
            -DMINIM=$<TARGET_FILE:minim>
            -DDIR=${dir}
            -DINPUT=${dir}/${name}.scm
            -DEXPECTED=${dir}/${name}.out
            -DEXPECTED_ERR=${dir}/${name}.err
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_minim.cmake)
endfunction()

# minim_add_script_test(name [STATUS n] [ARGS arg...]): runs `minim ARGS`
# (default `<name>.scm`) from scripts/, and checks the exit status (default 0),
# stdout against scripts/<name>.out, and stderr against scripts/<name>.err if
# it exists.
function(minim_add_script_test name)
    cmake_parse_arguments(PARSE_ARGV 1 opt "" "STATUS" "ARGS")
    if(NOT DEFINED opt_STATUS)
        set(opt_STATUS 0)
    endif()
    if(NOT opt_ARGS)
        set(opt_ARGS ${name}.scm)
    endif()
    string(REPLACE ";" "|" args "${opt_ARGS}")
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
    add_test(NAME script_${name}
        COMMAND ${CMAKE_COMMAND}
            -DMINIM=$<TARGET_FILE:minim>
            -DDIR=${dir}
            -DARGS=${args}
            -DSTATUS=${opt_STATUS}
            -DEXPECTED=${dir}/${name}.out
            -DEXPECTED_ERR=${dir}/${name}.err
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_minim.cmake)
endfunction()

minim_add_repl_test(chars)
minim_add_repl_test(echo)
minim_add_repl_test(equality)
minim_add_repl_test(eval)
minim_add_repl_test(hashing)
minim_add_repl_test(lists)
minim_add_repl_test(load)
minim_add_repl_test(prelude)
minim_add_repl_test(prims)
minim_add_repl_test(strings)
minim_add_repl_test(tail)

minim_add_script_test(fail STATUS 1)
minim_add_script_test(hello)
minim_add_script_test(missing STATUS 1)
minim_add_script_test(usage STATUS 2 ARGS a.scm b.scm)
