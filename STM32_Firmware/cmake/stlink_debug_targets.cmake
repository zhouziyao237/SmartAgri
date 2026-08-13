# Optional integration for a CMake firmware target.
# Example after your executable is created:
#   include(cmake/stlink_debug_targets.cmake)
#   smartagri_add_stlink_debug_targets(smart_agri_comm.elf)

set(OPENOCD_EXECUTABLE "" CACHE FILEPATH "Path to openocd executable")
set(OPENOCD_SCRIPTS_DIR "" CACHE PATH "Path to OpenOCD share/openocd/scripts")
set(STLINK_OPENOCD_CFG "${CMAKE_SOURCE_DIR}/openocd/stm32f103_stlink_swd.cfg" CACHE FILEPATH "ST-LINK OpenOCD cfg")

function(smartagri_add_stlink_debug_targets FIRMWARE_TARGET)
    if(NOT TARGET ${FIRMWARE_TARGET})
        message(FATAL_ERROR "ST-LINK integration: target '${FIRMWARE_TARGET}' does not exist")
    endif()

    if(NOT OPENOCD_EXECUTABLE)
        find_program(_SMARTAGRI_OPENOCD NAMES openocd openocd.exe
            HINTS
              "D:/GCC-ENV/Arm-GCC/openocd/bin"
              "D:/OpenOCD/bin"
              "C:/OpenOCD/bin"
              "C:/Program Files/OpenOCD/bin")
    else()
        set(_SMARTAGRI_OPENOCD "${OPENOCD_EXECUTABLE}")
    endif()

    if(NOT _SMARTAGRI_OPENOCD)
        message(STATUS "OpenOCD not found: ST-LINK custom targets will not be created. Set OPENOCD_EXECUTABLE.")
        return()
    endif()

    if(NOT EXISTS "${STLINK_OPENOCD_CFG}")
        message(FATAL_ERROR "ST-LINK OpenOCD cfg not found: ${STLINK_OPENOCD_CFG}")
    endif()

    set(_OCD_PREFIX "${_SMARTAGRI_OPENOCD}")
    if(OPENOCD_SCRIPTS_DIR)
        list(APPEND _OCD_PREFIX -s "${OPENOCD_SCRIPTS_DIR}")
    endif()
    list(APPEND _OCD_PREFIX -f "${STLINK_OPENOCD_CFG}")

    add_custom_target(stlink-server
        COMMAND ${_OCD_PREFIX}
        USES_TERMINAL
        COMMENT "Starting OpenOCD ST-LINK SWD server on GDB port 3333"
    )

    add_custom_target(stlink-flash
        COMMAND ${_OCD_PREFIX} -c "program {$<TARGET_FILE:${FIRMWARE_TARGET}>} verify reset exit"
        DEPENDS ${FIRMWARE_TARGET}
        USES_TERMINAL
        COMMENT "Programming ${FIRMWARE_TARGET} with ST-LINK/OpenOCD"
    )

    add_custom_target(stlink-reset
        COMMAND ${_OCD_PREFIX} -c "init; reset run; shutdown"
        USES_TERMINAL
        COMMENT "Resetting STM32F103 with ST-LINK/OpenOCD"
    )

    add_custom_target(stlink-halt
        COMMAND ${_OCD_PREFIX} -c "init; reset halt; shutdown"
        USES_TERMINAL
        COMMENT "Resetting and halting STM32F103 with ST-LINK/OpenOCD"
    )

    add_custom_target(stlink-erase
        COMMAND ${_OCD_PREFIX} -c "init; reset halt; stm32f1x mass_erase 0; reset halt; shutdown"
        USES_TERMINAL
        COMMENT "Mass-erasing STM32F103 with ST-LINK/OpenOCD"
    )
endfunction()
