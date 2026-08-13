#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "smartagri_protocol.h"
int main(void){
    uint8_t payload[3]={1,2,3},route_bytes[SA_MAX_PAYLOAD],frame_bytes[SA_MAX_FRAME];
    uint16_t route_len=0;
    assert(sa_route_pack(SA_DEVICE_PC,1001,1,SA_CMD_CAPS_GET,0,1,payload,sizeof(payload),route_bytes,&route_len));
    size_t n=sa_frame_encode(7,SA_CMD_ROUTE,route_bytes,route_len,frame_bytes,sizeof(frame_bytes));
    assert(n>0);
    sa_frame_parser_t parser;sa_frame_parser_init(&parser);sa_frame_t frame;
    assert(sa_frame_parser_feed(&parser,frame_bytes,n,&frame));
    sa_route_packet_t route;assert(sa_route_unpack(frame.payload,frame.payload_len,&route));
    assert(route.h.dst_device==1001&&route.h.inner_cmd==SA_CMD_CAPS_GET&&route.payload[2]==3);
    const char *txt="SERIAL_TEST";
    uint8_t troute[SA_MAX_PAYLOAD]; uint16_t troute_len=0;
    assert(sa_route_pack(1001,SA_DEVICE_MASTER,1,SA_CMD_LINK_TEST,0,1,txt,11,troute,&troute_len));
    sa_route_packet_t tp; assert(sa_route_unpack(troute,troute_len,&tp));
    assert(tp.h.inner_cmd==SA_CMD_LINK_TEST && tp.h.src_device==1001 && tp.h.payload_len==11);
    assert(memcmp(tp.payload,txt,11)==0);
    puts("PASS");return 0;
}
