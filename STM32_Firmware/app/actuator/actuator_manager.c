#include "actuator_manager.h"
#include "board.h"
typedef struct{uint8_t active;uint16_t pin;uint32_t until;uint8_t restore;} pulse_t;static pulse_t p[4];
void ActuatorManager_Init(void){for(int i=0;i<4;i++)p[i].active=0;}
bool ActuatorManager_Command(const sa_gpio_control_t*c){if(!c||!(Board_PinCapabilities(c->pin_id)&SA_PINCAP_GPIO_OUT)||(Board_PinCapabilities(c->pin_id)&SA_PINCAP_RESERVED))return false;if(!Board_GPIO_ConfigOutput(c->pin_id,c->value?GPIO_PIN_SET:GPIO_PIN_RESET))return false;if(c->mode==1&&c->duration_ms){for(int i=0;i<4;i++)if(!p[i].active){p[i].active=1;p[i].pin=c->pin_id;p[i].until=Board_Millis()+c->duration_ms;p[i].restore=!c->value;return true;}return false;}return true;}
void ActuatorManager_Process(void){uint32_t now=Board_Millis();for(int i=0;i<4;i++)if(p[i].active&&(int32_t)(now-p[i].until)>=0){Board_GPIO_Write(p[i].pin,p[i].restore?GPIO_PIN_SET:GPIO_PIN_RESET);p[i].active=0;}}
