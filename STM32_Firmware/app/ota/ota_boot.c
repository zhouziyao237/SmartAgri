#include "ota_boot.h"
#include "board.h"

#define OTA_BOOT_MAGIC      0xA55Au
#define OTA_SESSION_VERSION 0x0001u

static uint16_t ota_session_crc(uint16_t version, uint16_t dev_lo, uint16_t dev_hi,
                                uint16_t group, uint16_t tid_lo, uint16_t tid_hi)
{
    uint16_t c = 0x6D39u;
    const uint16_t w[6] = {version, dev_lo, dev_hi, group, tid_lo, tid_hi};
    for (unsigned i = 0; i < 6; ++i) {
        c ^= w[i];
        c = (uint16_t)((c << 5) | (c >> 11));
        c = (uint16_t)(c + 0x9E37u);
    }
    return c;
}

void OtaBoot_Request(uint32_t transfer_id, uint32_t device_id, uint16_t group_id)
{
    const uint16_t dev_lo = (uint16_t)(device_id & 0xFFFFu);
    const uint16_t dev_hi = (uint16_t)(device_id >> 16);
    const uint16_t tid_lo = (uint16_t)(transfer_id & 0xFFFFu);
    const uint16_t tid_hi = (uint16_t)(transfer_id >> 16);

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* DR1 is written last.  The bootloader only accepts the session when the
     * magic and checksum are valid, so a reset cannot observe a half-written
     * identity. */
    BKP->DR1 = 0u;
    BKP->DR2 = OTA_SESSION_VERSION;
    BKP->DR3 = dev_lo;
    BKP->DR4 = dev_hi;
    BKP->DR5 = group_id;
    BKP->DR6 = tid_lo;
    BKP->DR7 = tid_hi;
    BKP->DR8 = ota_session_crc(OTA_SESSION_VERSION, dev_lo, dev_hi, group_id, tid_lo, tid_hi);
    BKP->DR1 = OTA_BOOT_MAGIC;

    __DSB();
    __ISB();
    NVIC_SystemReset();
}
