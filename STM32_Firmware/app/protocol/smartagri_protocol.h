#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SA_SOF                 0xA5u
#define SA_FRAME_HEADER_LEN    5u
#define SA_CMD_ID_LEN          2u
#define SA_FRAME_TAIL_LEN      2u
#define SA_FRAME_OVERHEAD      9u
#define SA_MAX_PAYLOAD         512u
#define SA_MAX_FRAME           (SA_MAX_PAYLOAD + SA_FRAME_OVERHEAD)

#define SA_CMD_ROUTE           0x00E1u
#define SA_ROUTE_VERSION       2u
#define SA_DEVICE_LOCAL_ESP    0u /* local UART coprocessor control source; never routed over ESP-NOW */
#define SA_DEVICE_MASTER       1u
#define SA_DEVICE_PC           0xFFFFFFFEu
#define SA_DEVICE_BROADCAST    0xFFFFFFFFu

/* Inner command IDs */
enum {
    SA_CMD_LINK_HELLO          = 0x00F0,
    SA_CMD_LINK_ACK            = 0x00F1,
    SA_CMD_LINK_TEST           = 0x00F2,
    SA_CMD_CAPS_GET            = 0x3000,
    SA_CMD_CAPS_REPLY          = 0x3001,
    SA_CMD_CONFIG_GET          = 0x3002,
    SA_CMD_CONFIG_REPLY        = 0x3003,
    SA_CMD_CONFIG_BEGIN        = 0x3010,
    SA_CMD_CONFIG_SENSOR_SET   = 0x3011,
    SA_CMD_CONFIG_SENSOR_DEL   = 0x3012,
    SA_CMD_CONFIG_VALIDATE     = 0x3013,
    SA_CMD_CONFIG_COMMIT       = 0x3014,
    SA_CMD_CONFIG_ROLLBACK     = 0x3015,
    SA_CMD_CONFIG_FACTORY      = 0x3016,
    SA_CMD_NODE_SET            = 0x3020,
    SA_CMD_NODE_INFO           = 0x3021,
    SA_CMD_RADIO_CONFIG_GET    = 0x3030,
    SA_CMD_RADIO_CONFIG_REPLY  = 0x3031,
    SA_CMD_RADIO_CONFIG_SET    = 0x3032,
    SA_CMD_TELEMETRY           = 0x3100,
    SA_CMD_DIAG_TELEMETRY      = 0x31F0, /* diagnostic: node generates one real TELEMETRY uplink */
    SA_CMD_CONTROL_GPIO        = 0x3200,
    SA_CMD_CONTROL_RESULT      = 0x3201,
    SA_CMD_HEARTBEAT           = 0x3300,
    SA_CMD_OTA_INFO_GET        = 0x3400,
    SA_CMD_OTA_INFO_REPLY      = 0x3401,
    SA_CMD_OTA_PREPARE         = 0x3402,
    SA_CMD_OTA_BEGIN           = 0x3403,
    SA_CMD_OTA_DATA            = 0x3404,
    SA_CMD_OTA_END             = 0x3405,
    SA_CMD_OTA_ABORT           = 0x3406,
    SA_CMD_OTA_STATUS          = 0x3407,
    SA_CMD_OTA_RESULT          = 0x3408,
};

enum {
    SA_OTA_COMPONENT_STM32 = 1,
    SA_OTA_COMPONENT_ESP   = 2
};
enum {
    SA_OTA_TARGET_STM32F103C8 = 0x0101,
    SA_OTA_TARGET_ESP32       = 0x0201,
    SA_OTA_TARGET_ESP32C3     = 0x0202
};
enum {
    SA_OTA_STATE_IDLE=0, SA_OTA_STATE_READY=1, SA_OTA_STATE_RECEIVING=2,
    SA_OTA_STATE_VERIFYING=3, SA_OTA_STATE_REBOOT_PENDING=4, SA_OTA_STATE_ERROR=5
};

#pragma pack(push,1)
typedef struct { uint8_t component; uint8_t reserved[3]; } sa_ota_info_get_t;
typedef struct { uint8_t component; uint8_t state; uint16_t target; uint32_t fw_version; uint32_t max_image_size; uint32_t flags; } sa_ota_info_reply_t;
typedef struct { uint8_t component; uint8_t reserved[3]; uint32_t transfer_id; } sa_ota_prepare_t;
typedef struct { uint8_t component; uint8_t reserved; uint16_t target; uint32_t transfer_id; uint32_t image_size; uint32_t image_crc32; } sa_ota_begin_t;
typedef struct { uint8_t component; uint8_t reserved; uint16_t data_len; uint32_t transfer_id; uint32_t offset; } sa_ota_data_head_t;
typedef struct { uint8_t component; uint8_t reserved; uint16_t reserved2; uint32_t transfer_id; uint32_t image_crc32; } sa_ota_end_t;
typedef struct { uint8_t component; uint8_t reserved[3]; uint32_t transfer_id; } sa_ota_simple_t;
typedef struct { uint8_t component; uint8_t state; int16_t result; uint32_t transfer_id; uint32_t next_offset; uint32_t detail; } sa_ota_result_t;
#pragma pack(pop)

/* Link capability negotiation. LINK_HELLO/LINK_ACK command IDs remain
 * unchanged so older Protocol-2 firmware continues to interoperate. Older
 * nodes use a zero-length payload; newer nodes may attach this structure. */
#define SA_LINK_INFO_MAGIC       0x324B4E4Cu /* "LNK2" little-endian */
#define SA_PROTO_MIN_SUPPORTED   2u
#define SA_PROTO_MAX_SUPPORTED   2u
#define SA_FW_VERSION_CURRENT    0x00020407u

enum {
    SA_CAP_RADIO_CONFIG   = 1u << 0,
    SA_CAP_AUTO_DISCOVERY = 1u << 1,
    SA_CAP_CHANNEL_SCAN   = 1u << 2,
    SA_CAP_SENSOR_CONFIG  = 1u << 3,
    SA_CAP_OTA            = 1u << 4,
    SA_CAP_LEGACY_PROTO2  = 1u << 5
};

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint32_t fw_version;
    uint32_t capabilities;
    uint16_t target;
    uint8_t proto_min;
    uint8_t proto_max;
    uint8_t role;
    uint8_t channel;
    uint16_t reserved;
} sa_link_info_t;

#define SA_RADIO_CONFIG_SCHEMA 1u
typedef struct {
    uint16_t schema_version;
    uint8_t role;
    uint8_t serial_mode;
    uint32_t device_id;
    uint16_t network_id;
    uint16_t group_id;
    uint8_t channel;
    uint8_t power_mode;
    uint8_t uplink;
    uint8_t auto_discovery;
    uint8_t channel_scan;
    uint8_t gateway_valid;
    uint8_t reserved0[2];
    uint8_t gateway_mac[6];
    uint32_t fw_version;
    uint8_t proto_min;
    uint8_t proto_max;
    uint16_t capabilities;
} sa_radio_config_payload_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint8_t version;
    uint8_t flags;
    uint8_t priority;
    uint8_t reserved0;
    uint32_t src_device;
    uint32_t dst_device;
    uint16_t group_id;
    uint16_t inner_cmd;
    uint16_t payload_len;
    uint16_t reserved1;
} sa_route_header_t;
#pragma pack(pop)

#define SA_ROUTE_HEADER_LEN ((uint16_t)sizeof(sa_route_header_t))
#define SA_MAX_ROUTE_PAYLOAD (SA_MAX_PAYLOAD - SA_ROUTE_HEADER_LEN)

typedef struct {
    sa_route_header_t h;
    uint8_t payload[SA_MAX_ROUTE_PAYLOAD];
} sa_route_packet_t;

typedef struct {
    uint8_t seq;
    uint16_t cmd_id;
    uint16_t payload_len;
    uint8_t payload[SA_MAX_PAYLOAD];
} sa_frame_t;

typedef struct {
    uint8_t buf[SA_MAX_FRAME];
    size_t used;
    uint32_t good_frames;
    uint32_t crc8_fail;
    uint32_t crc16_fail;
    uint32_t length_fail;
    uint32_t resync_drop;
} sa_frame_parser_t;

uint8_t sa_crc8(const uint8_t *data, size_t len);
uint16_t sa_crc16_ccitt(const uint8_t *data, size_t len);
size_t sa_frame_encode(uint8_t seq, uint16_t cmd_id, const uint8_t *payload, uint16_t payload_len,
                       uint8_t *out, size_t out_cap);
void sa_frame_parser_init(sa_frame_parser_t *p);
/* Feed stream bytes. Returns true when one complete frame is produced. Call repeatedly with len=0 to drain buffered frames. */
bool sa_frame_parser_feed(sa_frame_parser_t *p, const uint8_t *data, size_t len, sa_frame_t *out);

bool sa_route_pack(uint32_t src, uint32_t dst, uint16_t group, uint16_t inner_cmd,
                   uint8_t flags, uint8_t priority, const void *payload, uint16_t payload_len,
                   uint8_t *out, uint16_t *out_len);
bool sa_route_unpack(const uint8_t *data, uint16_t len, sa_route_packet_t *out);

static inline uint16_t sa_get_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline void sa_put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xffu); p[1] = (uint8_t)(v >> 8);
}

#ifdef __cplusplus
}
#endif
