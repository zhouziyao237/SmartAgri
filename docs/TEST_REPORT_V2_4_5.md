# SmartAgri V2.4.5 Source Validation Report

Validation performed in packaging environment:

- Regression: READY no longer gates USART1 TX.
- Regression: USART1 TX path is blocking/deterministic while RX remains circular DMA.
- Regression: new TX counters are exported and present in `[STAT]`.
- Regression: V2.4.4 local PC decoupling remains present.
- Regression: ESP authoritative identity/config rules remain present.
- Regression: OTA session codec remains unchanged.

This environment does not contain the user's Windows ARM-GCC/OpenOCD or ESP-IDF installation, so the final ARM binary must be built with the user's existing toolchain and then verified on hardware.
