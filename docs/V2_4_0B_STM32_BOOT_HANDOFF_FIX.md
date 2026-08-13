# SmartAgri V2.4.0b STM32 boot hand-off fix

This revision changes only the STM32 OTA boot/application hand-off. ESP V2.4.0a does not need to be reflashed.

Fixes:
- Final jump is performed in a naked ARM routine; no C stack access occurs after MSP changes.
- SysTick is stopped before leaving the bootloader.
- NVIC enable/pending state is cleared.
- VTOR is switched to `0x08002000`.
- CONTROL/BASEPRI/FAULTMASK/PRIMASK are restored to reset-like values.
- Application explicitly enables IRQs before `HAL_Init()`, preventing a frozen HAL tick if a prior stage left PRIMASK set.

The Flash map is unchanged:
- Bootloader: `0x08000000` .. `0x08001FFF`
- Application: `0x08002000` .. `0x0800F7FF`
- Config A: `0x0800F800`
- Config B: `0x0800FC00`
