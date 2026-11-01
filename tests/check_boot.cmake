# Fails unless BOOT_GEN, run on SRC/prelude.scm and SRC/expander.scm, writes
# SRC/prelude.boot.
# Usage: cmake -DBOOT_GEN=... -DSRC=... -DOUTPUT=... -P check_boot.cmake
execute_process(
    COMMAND ${BOOT_GEN} ${SRC}/boot.scm ${OUTPUT} ${SRC}/prelude.scm ${SRC}/expander.scm
    RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "boot_gen failed")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${OUTPUT} ${SRC}/prelude.boot
    RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "src/prelude.boot is out of date; build the `boot` target")
endif()
