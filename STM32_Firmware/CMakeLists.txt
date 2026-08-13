cmake_minimum_required(VERSION 3.20)
if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/arm-gcc.cmake" CACHE FILEPATH "" FORCE)
endif()
project(SmartAgri_F103C8_V2_4_4_OTA C ASM)
set(CMAKE_C_STANDARD 11)

# Clock policy: UICRM F103_Nano_general-compatible HSE 8 MHz -> PLL x9 -> 72 MHz first.
# If the board HSE does not become ready, AUTO mode retries then falls back to
# HSI/2 x16 = 64 MHz so the node can still boot for diagnostics/bring-up.
option(SMARTAGRI_CLOCK_STRICT_HSE "Fail fatally instead of HSI64 fallback when HSE startup fails" OFF)
set(SMARTAGRI_HSE_RETRY_COUNT 3 CACHE STRING "Number of HSE startup attempts before fallback/fatal")

set(BOARD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/boards/base/SmartAgri_F103C8")
include(cmake/board_local_cube.cmake)
smartagri_prepare_board_local_cube("${BOARD_DIR}")

# IMPORTANT: everything below is board-local. No D:/STM32Cube/... source path is
# ever added to smart_agri_comm, so CLion keeps one clean project tree.
set(DRIVERS_DIR "${BOARD_DIR}/Drivers")
set(HAL_DIR "${DRIVERS_DIR}/STM32F1xx_HAL_Driver")
set(CMSIS_DIR "${DRIVERS_DIR}/CMSIS")
set(DEVICE_DIR "${CMSIS_DIR}/Device/ST/STM32F1xx")

set(CPU_FLAGS -mcpu=cortex-m3 -mthumb)
set(HAL_SRC
  ${HAL_DIR}/Src/stm32f1xx_hal.c
  ${HAL_DIR}/Src/stm32f1xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_dma.c
  ${HAL_DIR}/Src/stm32f1xx_hal_flash.c
  ${HAL_DIR}/Src/stm32f1xx_hal_flash_ex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32f1xx_hal_gpio_ex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_pwr.c
  ${HAL_DIR}/Src/stm32f1xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32f1xx_hal_rcc_ex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_uart.c
  ${HAL_DIR}/Src/stm32f1xx_hal_i2c.c
  ${HAL_DIR}/Src/stm32f1xx_hal_adc.c
  ${HAL_DIR}/Src/stm32f1xx_hal_adc_ex.c
)

set(STARTUP "${DEVICE_DIR}/Source/Templates/gcc/startup_stm32f103xb.s")
set(SYSTEM  "${DEVICE_DIR}/Source/Templates/system_stm32f1xx.c")
foreach(_required IN LISTS HAL_SRC)
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "Required board-local HAL source missing: ${_required}")
  endif()
endforeach()
if(NOT EXISTS "${STARTUP}" OR NOT EXISTS "${SYSTEM}")
  message(FATAL_ERROR "Board-local CMSIS startup/system source missing under ${DEVICE_DIR}/Source/Templates")
endif()

add_executable(smart_agri_comm
  ${STARTUP}
  ${SYSTEM}
  ${HAL_SRC}
  boards/base/SmartAgri_F103C8/Src/board.c
  app/main.c
  app/log/debug_log.c
  app/log/debug_shell.c
  app/protocol/smartagri_protocol.c
  app/comm/comm_link.c
  app/comm/pc_link.c
  app/config/config_manager.c
  app/sensor/sensor_manager.c
  app/actuator/actuator_manager.c
  app/node/node_service.c
  app/ota/ota_boot.c
)
set_target_properties(smart_agri_comm PROPERTIES SUFFIX ".elf")
target_compile_definitions(smart_agri_comm PRIVATE
  USE_HAL_DRIVER STM32F103xB
  SMARTAGRI_HSE_RETRY_COUNT=${SMARTAGRI_HSE_RETRY_COUNT})
if(SMARTAGRI_CLOCK_STRICT_HSE)
  target_compile_definitions(smart_agri_comm PRIVATE SMARTAGRI_CLOCK_STRICT_HSE=1)
else()
  target_compile_definitions(smart_agri_comm PRIVATE SMARTAGRI_CLOCK_STRICT_HSE=0)
endif()
target_include_directories(smart_agri_comm PRIVATE
  boards/base/SmartAgri_F103C8/Inc
  app/log app/protocol app/comm app/config app/sensor app/actuator app/node app/ota
  ${HAL_DIR}/Inc
  ${HAL_DIR}/Inc/Legacy
  ${CMSIS_DIR}/Include
  ${DEVICE_DIR}/Include
)
target_compile_options(smart_agri_comm PRIVATE
  ${CPU_FLAGS}
  -ffunction-sections -fdata-sections
  -Wall -Wextra -Wshadow
  -g3 -Og)
target_link_options(smart_agri_comm PRIVATE
  ${CPU_FLAGS}
  -T${BOARD_DIR}/STM32F103C8TX_APP_OTA.ld
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/smart_agri_comm.map
  --specs=nano.specs --specs=nosys.specs)

if(CMAKE_OBJCOPY)
  add_custom_command(TARGET smart_agri_comm POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:smart_agri_comm> ${CMAKE_BINARY_DIR}/smart_agri_comm.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:smart_agri_comm> ${CMAKE_BINARY_DIR}/smart_agri_comm.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:smart_agri_comm>)
endif()

# V2.4.0 OTA bootloader: fixed 8 KB at 0x08000000.
# The universal application is linked at 0x08002000 and can then be updated remotely.
add_executable(smart_agri_bootloader
  bootloader/startup_ota.s
  bootloader/bootloader.c
)
set_target_properties(smart_agri_bootloader PROPERTIES SUFFIX ".elf")
target_compile_options(smart_agri_bootloader PRIVATE
  ${CPU_FLAGS}
  -ffreestanding -fno-builtin
  -ffunction-sections -fdata-sections
  -Wall -Wextra -Wshadow
  -Os -g3)
target_link_options(smart_agri_bootloader PRIVATE
  ${CPU_FLAGS}
  -T${CMAKE_CURRENT_SOURCE_DIR}/bootloader/STM32F103C8_BOOT_8K.ld
  -nostdlib
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/smart_agri_bootloader.map)

if(CMAKE_OBJCOPY)
  add_custom_command(TARGET smart_agri_bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:smart_agri_bootloader> ${CMAKE_BINARY_DIR}/smart_agri_bootloader.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:smart_agri_bootloader> ${CMAKE_BINARY_DIR}/smart_agri_bootloader.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:smart_agri_bootloader>)
endif()

add_custom_target(smart_agri_ota_bundle ALL
  DEPENDS smart_agri_bootloader smart_agri_comm
  COMMENT "Build SmartAgri STM32 OTA bootloader + relocated application")

include(cmake/stlink_debug_targets.cmake)
smartagri_add_stlink_debug_targets(smart_agri_comm)
