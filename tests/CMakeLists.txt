function(minim_add_test name)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE minim_core)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

minim_add_test(smoke)
minim_add_test(object)
