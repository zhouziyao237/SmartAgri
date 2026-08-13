#include "debug_log.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
static char g_log_buf[256];
void DebugLog_Init(void){}
void DebugLog_Printf(const char *fmt, ...){
    va_list ap;va_start(ap,fmt);int n=vsnprintf(g_log_buf,sizeof(g_log_buf),fmt,ap);va_end(ap);
    if(n<=0)return;
    if(n>=(int)sizeof(g_log_buf))n=(int)sizeof(g_log_buf)-1;
    HAL_UART_Transmit(&huart1,(uint8_t*)g_log_buf,(uint16_t)n,100);
}
