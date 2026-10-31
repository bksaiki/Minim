# Pipes INPUT into MINIM and compares its stdout with EXPECTED, and its stderr
# with EXPECTED_ERR if that file exists.
execute_process(
    COMMAND ${MINIM}
    INPUT_FILE ${INPUT}
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE errors
    RESULT_VARIABLE status
)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "minim exited with ${status}\n${errors}")
endif()
file(READ ${EXPECTED} expected)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "output differs\n--- expected\n${expected}--- actual\n${actual}--- stderr\n${errors}")
endif()
if(EXISTS ${EXPECTED_ERR})
    file(READ ${EXPECTED_ERR} expected_errors)
    if(NOT errors STREQUAL expected_errors)
        message(FATAL_ERROR "stderr differs\n--- expected\n${expected_errors}--- actual\n${errors}")
    endif()
endif()
