#pragma once
#include <stdbool.h>
#include <stdint.h>

void NodeService_Init(void);
void NodeService_Process(void);

/* Runtime RADIO identity becomes ready only after a local attached-ESP NODE_SET.
 * Persisted identity never activates ESP/radio routing. V2.4.4 permits a valid
 * persisted MASTER identity to keep the directly attached USART2 PC management
 * channel available for diagnostics/local management while radio is WAIT_ESP. */
bool NodeService_IdentityReady(void);
bool NodeService_IdentityFromEsp(void);
uint32_t NodeService_LastEspIdentityMs(void);
