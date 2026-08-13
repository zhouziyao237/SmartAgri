# Usage:
# arm-none-eabi-gdb.exe <your.elf> -x gdb/stm32f103_stlink.gdb

target extended-remote localhost:3333
monitor reset halt

# Program the ELF to Flash, verify through the debugger, then halt at reset.
load
monitor reset halt

tbreak main
continue
