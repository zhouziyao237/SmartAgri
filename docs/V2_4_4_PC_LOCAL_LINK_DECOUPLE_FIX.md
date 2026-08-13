# V2.4.4 PC Local Link Decouple Fix

## Root cause

V2.4.3 `on_route_from_pc()` began with:

```c
if(!r || !identity_ready) return;
```

This coupled the local USART2 management plane to attached-ESP identity readiness. A MASTER STM32 with a valid persisted identity could print debug text over USART2 but could not answer the upper-computer A5 HEARTBEAT probe while UART1/ESP was unavailable.

## Fix

V2.4.4 introduces two independent readiness domains:

- **Local PC management:** may use a valid persisted MASTER identity.
- **ESP/radio forwarding:** still requires authoritative `NODE_SET` from the attached ESP.

The WAIT_ESP branch executes local commands only and returns before any `CommLink_SendRoute()` forwarding path.

## Diagnostics

`pc_link.c` now records raw USART2 bytes and parser/error counters. This distinguishes:

- `pc_raw=0`: no bytes reached PA3;
- `pc_raw>0, pc_ok=0`: bytes arrived but A5 parse failed;
- `pc_ok>0`: upper-computer A5 ingress is valid;
- `pc_tx>0`: STM32 emitted A5 replies.

## Compatibility

ESP V2.4.2 is protocol-compatible and does not require reflashing for this fix.
