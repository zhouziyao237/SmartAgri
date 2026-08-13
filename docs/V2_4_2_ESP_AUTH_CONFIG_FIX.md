# SmartAgri V2.4.2 — ESP Authoritative Configuration Fix

## Design rule

The STM32 application is one universal image. MASTER / SLAVE and logical node identity are owned by the attached ESP configuration and are synchronized over the business UART with `SA_CMD_NODE_SET`.

- ESP `ROLE=MASTER, DEVICE=1` -> attached STM32 becomes MASTER / Node 1.
- ESP `ROLE=SLAVE, DEVICE=1001` -> attached STM32 becomes SLAVE / Node 1001.
- Swapping ESP roles does not require rebuilding or reflashing a different STM32 application image.

## Configuration fixes in V2.4.2

1. STM32 compiled defaults are now `UNCONFIGURED/0/0/0`, not a fake `SLAVE/1001` identity.
2. STM32 role-sensitive runtime routing stays `WAIT_ESP` until attached-ESP identity arrives. A persisted identity is retained only for bootloader/OTA recovery and is never activated as App MASTER/SLAVE by itself.
3. `NODE_SET` is accepted only from `SA_DEVICE_LOCAL_ESP=0` on USART1. PC broadcasts and remote nodes cannot change STM32 identity.
4. Sensor configuration transactions can no longer overwrite a newer ESP identity. Identity fields are merged from the active config at sensor commit.
5. Sensor factory reset preserves role/device/network/group instead of resetting the node identity.
6. An open sensor transaction is updated when the ESP changes node identity, preventing later rollback to stale identity fields.
7. STM32 Bootloader no longer assumes Device 1 when it has no valid config/session; Device 0 means unconfigured.
8. ESP AT configuration is staged. Editing `ROLE/DEVICE/NETWORK/GROUP/CHANNEL/...` does not alter the running radio or STM32 identity until `AT+APPLY`.
9. `AT+APPLY` validates, saves to NVS, then restarts. `AT+SAVE` validates and saves but reports that APPLY is still required.
10. ESP rejects invalid combinations such as MASTER with Device != 1, SLAVE with Device 1, SLAVE without Gateway, Network 0, invalid channel/ranges, and malformed numeric AT values.
11. Existing valid ESP NVS configuration is retained because the NVS layout version has not changed.
12. Added `AT+PENDING?`, `AT+CONFIGCHECK`, `AT+BUS?`, and `AT+SYNCSTM32` diagnostics.
13. ESP business-UART counters expose TX/RX/parse-drop information so `STM32 esp_rx=0` can be isolated quickly.
14. STM32 debug shell reports identity readiness/source, persistence state, transaction state, and last ESP identity sync time.
15. STM32/ESP OTA version is `0x00020402`; OTA session + Bootloader handoff fixes from V2.4.1 are retained.

## Expected boot sequence

STM32 after power-up may first show the persisted fallback:

```
[CFG-FALLBACK] role=... device=... persisted=1
[IDENTITY] waiting for attached ESP NODE_SET; persisted identity is bootloader-only until ESP sync
```

When the attached ESP business UART is working, during the startup identity burst (first send at about 0.6 s, repeated four times):

```
[IDENTITY] source=ESP role=1->2 device=1->1001 network=1->1 group=1->1 gen=...
```

Then `status` on STM32 should show:

```
identity.ready=1 source=ESP
esp.rx=>0
```

On ESP:

```
AT+STATUS?    # active/applied config
AT+PENDING?   # staged AT edits
AT+BUS?       # STM32 business UART counters
AT+SYNCSTM32  # force one identity sync using active config
```

## OTA

V2.4.2 retains the V2.4.1 OTA session mechanism. STM32 OTA uses `smart_agri_comm.bin`; ESP OTA uses the target-specific V2.4.2 application bin.
