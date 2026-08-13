# Usage:
# arm-none-eabi-gdb.exe cmake-build-debug/smart_agri_comm.elf -x gdb/stm32f103_app_direct.gdb
#
# This keeps the production OTA application linked at 0x08002000, but starts it
# directly for source-level debugging when the bootloader at 0x08000000 is not
# present or not part of the current debug session.

target extended-remote localhost:3333
monitor reset halt

load
set $sp = *(unsigned int*)0x08002000
set $pc = *(unsigned int*)0x08002004
set *(unsigned int*)0xE000ED08 = 0x08002000

tbreak main
continue
