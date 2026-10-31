# minim_add_test(name [INTERNAL]): INTERNAL tests may include headers from src/.
function(minim_add_test name)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE minim_core)
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
minim_add_test(read)

# minim_add_repl_test(name): pipes repl/<name>.scm into minim and compares
# stdout with repl/<name>.out.
function(minim_add_repl_test name)
    set(dir ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME repl_${name}
        COMMAND ${CMAKE_COMMAND}
            -DMINIM=$<TARGET_FILE:minim>
            -DINPUT=${dir}/repl/${name}.scm
            -DEXPECTED=${dir}/repl/${name}.out
            -P ${dir}/run_repl.cmake)
endfunction()

minim_add_repl_test(echo)
