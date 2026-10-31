# minim_add_test(name [INTERNAL]): INTERNAL tests may include headers from src/.
function(minim_add_test name)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE minim_core)
    if("INTERNAL" IN_LIST ARGN)
        target_include_directories(test_${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

minim_add_test(smoke)
minim_add_test(object)
minim_add_test(heap INTERNAL)
