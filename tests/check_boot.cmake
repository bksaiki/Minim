# Fails unless BOOT_GEN, run with BOOT_SCM on SOURCES (separated by |), writes
# EXPECTED.
# Usage: cmake -DBOOT_GEN=... -DBOOT_SCM=... -DSOURCES=... -DEXPECTED=... -DOUTPUT=...
#        -P check_boot.cmake
string(REPLACE "|" ";" sources "${SOURCES}")
execute_process(
    COMMAND ${BOOT_GEN} ${BOOT_SCM} ${OUTPUT} ${sources}
    RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "boot_gen failed")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${OUTPUT} ${EXPECTED}
    RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${EXPECTED} is out of date; build the `boot` target")
endif()
