/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "usb.h"

#if MICROPY_HW_ENABLE_USBDEV

#include "esp_mac.h"
#include "esp_rom_gpio.h"
#include "esp_private/usb_phy.h"

#include "shared/tinyusb/mp_usbd.h"
#include "shared/tinyusb/tusb_config.h"

static usb_phy_handle_t phy_hdl;

#define CALSCI_USB_MAC_BYTES (6)

static void calsci_runtime_get_mac_bytes(uint8_t *mac) {
    esp_efuse_mac_get_default(mac);
}

void usb_phy_init(void) {
    // ref: https://github.com/espressif/esp-usb/blob/4b6a798d0bed444fff48147c8dcdbbd038e92892/device/esp_tinyusb/tinyusb.c

    // Configure USB PHY
    static const usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .target = USB_PHY_TARGET_INT,
    };

    // Init ESP USB Phy
    usb_new_phy(&phy_conf, &phy_hdl);
}

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
void usb_usj_mode(void) {
    // Switch the USB PHY back to Serial/Jtag mode, disabling OTG support
    // This should be run before jumping to bootloader.
    usb_del_phy(phy_hdl);
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_SERIAL_JTAG,
    };
    usb_new_phy(&phy_conf, &phy_hdl);
}
#endif

#if CALSCI_RUNTIME_HAS_DYNAMIC_USB_STRINGS
static void calsci_runtime_format_usb_name(char *buf, size_t len) {
    uint8_t mac[CALSCI_USB_MAC_BYTES];
    calsci_runtime_get_mac_bytes(mac);
    MP_STATIC_ASSERT((sizeof(CALSCI_RUNTIME_USB_NAME_PREFIX) - 1) + CALSCI_USB_MAC_BYTES * 2 <= MICROPY_HW_USB_DESC_STR_MAX);
    snprintf(
        buf,
        len,
        CALSCI_RUNTIME_USB_NAME_PREFIX "%02X%02X%02X%02X%02X%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
}

bool calsci_runtime_get_usb_dynamic_string(uint8_t index, char *buf, size_t len) {
    if (buf == NULL || len == 0) {
        return false;
    }

    switch (index) {
        case USBD_STR_PRODUCT:
        case USBD_STR_CDC:
            calsci_runtime_format_usb_name(buf, len);
            return true;
        default:
            return false;
    }
}
#endif

void mp_usbd_port_get_serial_number(char *serial_buf) {
    // use factory default MAC as serial ID
    uint8_t mac[CALSCI_USB_MAC_BYTES];
    calsci_runtime_get_mac_bytes(mac);
    MP_STATIC_ASSERT(CALSCI_USB_MAC_BYTES * 2 <= MICROPY_HW_USB_DESC_STR_MAX);
    mp_usbd_hex_str(serial_buf, mac, sizeof(mac));
}

#endif // MICROPY_HW_ENABLE_USBDEV
