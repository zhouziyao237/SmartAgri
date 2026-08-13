#include "smartagri_protocol.h"
#include <string.h>

/* CRC-8/ATM: poly=0x07 init=0xFF, non-reflected */
uint8_t sa_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}

/* CRC-16/CCITT-FALSE: poly=0x1021 init=0xFFFF, non-reflected */
uint16_t sa_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t sa_frame_encode(uint8_t seq, uint16_t cmd_id, const uint8_t *payload, uint16_t payload_len,
                       uint8_t *out, size_t out_cap) {
    size_t total = SA_FRAME_OVERHEAD + payload_len;
    if (!out || payload_len > SA_MAX_PAYLOAD || out_cap < total) return 0;
    out[0] = SA_SOF;
    sa_put_u16_le(&out[1], payload_len);
    out[3] = seq;
    out[4] = sa_crc8(out, 4);
    sa_put_u16_le(&out[5], cmd_id);
    if (payload_len && payload) memcpy(&out[7], payload, payload_len);
    uint16_t crc = sa_crc16_ccitt(out, total - 2);
    sa_put_u16_le(&out[total - 2], crc);
    return total;
}

void sa_frame_parser_init(sa_frame_parser_t *p) {
    if (p) memset(p, 0, sizeof(*p));
}

static void drop_prefix(sa_frame_parser_t *p, size_t n) {
    if (n >= p->used) { p->used = 0; return; }
    memmove(p->buf, p->buf+n, p->used-n); p->used -= n;
}

bool sa_frame_parser_feed(sa_frame_parser_t *p, const uint8_t *data, size_t len, sa_frame_t *out) {
    if (!p || !out) return false;
    if (data && len) {
        if (len > sizeof(p->buf) - p->used) {
            size_t need = len - (sizeof(p->buf) - p->used);
            if (need >= p->used) p->used = 0;
            else drop_prefix(p, need);
            p->resync_drop += (uint32_t)need;
        }
        if (len > sizeof(p->buf)) { data += len - sizeof(p->buf); len = sizeof(p->buf); p->used = 0; }
        memcpy(p->buf + p->used, data, len); p->used += len;
    }
    for (;;) {
        if (p->used < SA_FRAME_OVERHEAD) return false;
        size_t sof = 0;
        while (sof < p->used && p->buf[sof] != SA_SOF) ++sof;
        if (sof) { p->resync_drop += (uint32_t)sof; drop_prefix(p, sof); }
        if (p->used < SA_FRAME_OVERHEAD) return false;
        if (sa_crc8(p->buf, 4) != p->buf[4]) { p->crc8_fail++; drop_prefix(p,1); continue; }
        uint16_t plen = sa_get_u16_le(&p->buf[1]);
        if (plen > SA_MAX_PAYLOAD) { p->length_fail++; drop_prefix(p,1); continue; }
        size_t total = SA_FRAME_OVERHEAD + plen;
        if (p->used < total) return false;
        uint16_t got = sa_get_u16_le(&p->buf[total-2]);
        uint16_t exp = sa_crc16_ccitt(p->buf, total-2);
        if (got != exp) { p->crc16_fail++; drop_prefix(p,1); continue; }
        out->seq = p->buf[3]; out->cmd_id = sa_get_u16_le(&p->buf[5]); out->payload_len = plen;
        if (plen) memcpy(out->payload, &p->buf[7], plen);
        drop_prefix(p,total); p->good_frames++; return true;
    }
}

bool sa_route_pack(uint32_t src, uint32_t dst, uint16_t group, uint16_t inner_cmd,
                   uint8_t flags, uint8_t priority, const void *payload, uint16_t payload_len,
                   uint8_t *out, uint16_t *out_len) {
    if (!out || !out_len || payload_len > SA_MAX_ROUTE_PAYLOAD) return false;
    sa_route_header_t h = {0};
    h.version=SA_ROUTE_VERSION; h.flags=flags; h.priority=priority; h.src_device=src; h.dst_device=dst;
    h.group_id=group; h.inner_cmd=inner_cmd; h.payload_len=payload_len;
    memcpy(out,&h,sizeof(h)); if (payload_len && payload) memcpy(out+sizeof(h),payload,payload_len);
    *out_len=(uint16_t)(sizeof(h)+payload_len); return true;
}

bool sa_route_unpack(const uint8_t *data, uint16_t len, sa_route_packet_t *out) {
    if (!data || !out || len < sizeof(sa_route_header_t)) return false;
    memcpy(&out->h,data,sizeof(out->h));
    if (out->h.version != SA_ROUTE_VERSION || out->h.payload_len > SA_MAX_ROUTE_PAYLOAD ||
        len != (uint16_t)(sizeof(sa_route_header_t)+out->h.payload_len)) return false;
    if (out->h.payload_len) memcpy(out->payload,data+sizeof(sa_route_header_t),out->h.payload_len);
    return true;
}
