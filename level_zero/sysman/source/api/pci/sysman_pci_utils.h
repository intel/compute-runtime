/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace L0 {
namespace Sysman {

// Conventional PCI and PCI-X Mode 1 devices have 256 bytes of
// configuration space.  PCI-X Mode 2 and PCIe devices have 4096 bytes of
// configuration space.
#define PCI_CFG_SPACE_SIZE 256      // Conventional PCI and PCI-X Mode 1 devices
#define PCI_CFG_SPACE_EXP_SIZE 4096 // PCI-X Mode 2 and PCIe devices

// PCI device status register
#define PCI_STATUS 0x06          // 16 bit status register of PCI device
#define PCI_STATUS_CAP_LIST 0x10 // Support Capability List

// PCI configuration space header
#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_SUBSYSTEM_VENDOR_ID 0x2c
#define PCI_SUBSYSTEM_DEVICE_ID 0x2e
#define PCI_INVALID_VENDOR_ID 0xffff // every config read returns all ones once a device stops responding

// Capability lists
#define PCI_CAPABILITY_LIST 0x34 // Offset of first capability list entry
#define PCI_CAP_LIST_ID 0        // Capability ID
#define PCI_CAP_ID_EXP 0x10      // PCI Express
#define PCI_CAP_LIST_NEXT 1      /* Next capability in the list */
#define PCI_CAP_FLAGS 2          // Capability defined flags (16 bits)

// Express Capabilities
#define PCI_EXP_TYPE_RC_END 0x9    // Root Complex Integrated Endpoint
#define PCI_EXP_TYPE_RC_EC 0xa     // Root Complex Event Collector
#define PCI_EXP_SLTCTL 24          // Slot Control
#define PCI_EXP_SLTCTL_HPIE 0x0020 // Hot-Plug Interrupt Enable
#define PCI_EXP_FLAGS_VERS 0x000f  // capability version bits of the PCI Express capabilities register
#define PCI_EXP_VERS_LNKCAP2 2     // capability version that introduced the Link Capabilities 2 register

// Bridge control
#define PCI_BRIDGE_CONTROL 0x3e       // Bridge control register
#define PCI_BRIDGE_CTL_BUS_RESET 0x40 // Secondary bus reset

// Link Capabilities
#define PCI_EXP_LNKCAP 12              // offset to link capabilities
#define PCI_EXP_LNK_STATUS 18          // offset to link status
#define PCI_EXP_LNKCAP2 44             // offset to link capabilities 2
#define PCI_EXP_LNKCAP2_SLS 0x000000fe // supported link speeds vector, bits 1 to 7

// Resizable BARs
#define PCI_REBAR_CAP 4                // capability register
#define PCI_REBAR_CAP_SIZES 0x00FFFFF0 // supported BAR sizes
#define PCI_REBAR_CTRL 8               // control register

// Extended Capabilities
#define PCI_EXT_CAP_ID_REBAR 0x15    // Resizable BAR
#define PCI_EXT_CAP_ID_VF_REBAR 0x24 // VF Resizable BAR

#define PCI_EXT_CAP_ID(header) (header & 0x0000ffff)
#define PCI_EXT_CAP_VER(header) ((header >> 16) & 0xf)
#define PCI_EXT_CAP_NEXT(header) ((header >> 20) & 0xffc)

class PciUtil {
  public:
    static inline uint32_t getDwordFromConfig(uint32_t pos, uint8_t *configMemory) {
        return configMemory[pos] | (configMemory[pos + 1] << 8) |
               (configMemory[pos + 2] << 16) | (configMemory[pos + 3] << 24);
    }
    static inline uint16_t getWordFromConfig(uint32_t pos, uint8_t *configMem) {
        return configMem[pos] | (configMem[pos + 1] << 8);
    }
    static inline uint8_t getByteFromConfig(uint32_t pos, uint8_t *configMem) {
        return configMem[pos];
    }
};

} // namespace Sysman
} // namespace L0
