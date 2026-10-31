# Runs MINIM from DIR with ARGS (separated by `|`) and INPUT on stdin, both
# optional.  Checks the exit status against STATUS (default 0), stdout against
# EXPECTED, and stderr against EXPECTED_ERR if that file exists.
if(NOT DEFINED STATUS)
    set(STATUS 0)
endif()
string(REPLACE "|" ";" args "${ARGS}")
if(DEFINED INPUT)
    set(input INPUT_FILE ${INPUT})
endif()
execute_process(
    COMMAND ${MINIM} ${args}
    ${input}
    WORKING_DIRECTORY ${DIR}
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE errors
    RESULT_VARIABLE status
)
if(NOT status EQUAL STATUS)
    message(FATAL_ERROR "minim exited with ${status}, expected ${STATUS}\n${errors}")
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
