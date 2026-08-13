# SmartAgri board-local STM32CubeF1 vendor preparation.
# Goal: CMake target sources always point inside this project, so CLion does not
# show D:/STM32Cube/Repository/... as an external source root.

set(STM32CUBE_F1_REPOSITORY "" CACHE PATH
    "STM32CubeF1 V1.8.6 repository used only to COPY vendor files into the board directory")

function(smartagri_prepare_board_local_cube BOARD_DIR)
    set(_hal_adc "${BOARD_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c")
    set(_device_h "${BOARD_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include/stm32f103xb.h")
    set(_core_h "${BOARD_DIR}/Drivers/CMSIS/Include/core_cm3.h")

    if(EXISTS "${_hal_adc}" AND EXISTS "${_device_h}" AND EXISTS "${_core_h}")
        message(STATUS "STM32CubeF1 mode       : BOARD-LOCAL")
        message(STATUS "STM32CubeF1 board root : ${BOARD_DIR}")
        return()
    endif()

    set(_candidates "")
    if(STM32CUBE_F1_REPOSITORY)
        list(APPEND _candidates "${STM32CUBE_F1_REPOSITORY}")
    endif()
    # User's currently installed location (seen in CLion) plus common CubeMX locations.
    list(APPEND _candidates
        "D:/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6"
        "C:/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6")
    if(DEFINED ENV{USERPROFILE})
        list(APPEND _candidates
            "$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6"
            "$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5")
    endif()

    set(_repo "")
    foreach(_r IN LISTS _candidates)
        if(EXISTS "${_r}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c" AND
           EXISTS "${_r}/Drivers/CMSIS/Device/ST/STM32F1xx/Include/stm32f103xb.h" AND
           EXISTS "${_r}/Drivers/CMSIS/Include/core_cm3.h")
            set(_repo "${_r}")
            break()
        endif()
    endforeach()

    if(NOT _repo)
        message(FATAL_ERROR
            "Board-local STM32CubeF1 Drivers are missing and STM32Cube FW_F1 repository was not found.\n"
            "Install STM32Cube FW_F1 V1.8.6 with CubeMX, or pass:\n"
            "  -DSTM32CUBE_F1_REPOSITORY=D:/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6\n"
            "Then reload CMake. The files will be COPIED into boards/base/SmartAgri_F103C8/Drivers.")
    endif()

    message(STATUS "STM32CubeF1 vendor repo : ${_repo}")
    message(STATUS "Vendoring HAL/CMSIS into board-local Drivers ...")

    file(MAKE_DIRECTORY "${BOARD_DIR}/Drivers")
    file(MAKE_DIRECTORY "${BOARD_DIR}/Drivers/CMSIS")

    # Copy the complete HAL component, CMSIS core headers, and STM32F1 device component.
    # After this copy all compilation paths are board-local.
    file(COPY "${_repo}/Drivers/STM32F1xx_HAL_Driver"
         DESTINATION "${BOARD_DIR}/Drivers")
    file(COPY "${_repo}/Drivers/CMSIS/Include"
         DESTINATION "${BOARD_DIR}/Drivers/CMSIS")
    file(MAKE_DIRECTORY "${BOARD_DIR}/Drivers/CMSIS/Device/ST")
    file(COPY "${_repo}/Drivers/CMSIS/Device/ST/STM32F1xx"
         DESTINATION "${BOARD_DIR}/Drivers/CMSIS/Device/ST")

    if(NOT EXISTS "${_hal_adc}" OR NOT EXISTS "${_device_h}" OR NOT EXISTS "${_core_h}")
        message(FATAL_ERROR "STM32CubeF1 board-local vendor copy failed.")
    endif()

    file(WRITE "${BOARD_DIR}/Drivers/.smartagri_vendor_info.txt"
        "Source repository: ${_repo}\n"
        "Copied at CMake configure time. Build uses only board-local paths.\n")

    message(STATUS "STM32CubeF1 mode       : BOARD-LOCAL (vendor copy completed)")
    message(STATUS "STM32CubeF1 board root : ${BOARD_DIR}")
endfunction()
