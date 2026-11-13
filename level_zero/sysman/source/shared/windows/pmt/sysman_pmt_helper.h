/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstdint>
#include <initguid.h>

namespace L0 {
namespace Sysman {
namespace PmtSysman {

constexpr uint32_t fileDeviceIntelPmt = 0x9998;
constexpr uint32_t anySizeArray = 1;
constexpr uint32_t methodBuffered = 0;
constexpr uint32_t fileReadAccess = 0x0001;

// Intel PMT Telemetry Interface GUID {3dfb2563-5c44-4c59-8d80-baea7d06e6b8}
constexpr GUID guidInterfacePmtTelemetry = {0x3dfb2563, 0x5c44, 0x4c59, {0x8d, 0x80, 0xba, 0xea, 0x7d, 0x06, 0xe6, 0xb8}};
constexpr GUID guidInterfaceDisplayAdapter = {0x4d36e968, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

inline constexpr uint32_t getCtlCode(uint32_t code) {
    return CTL_CODE(fileDeviceIntelPmt, code, methodBuffered, fileReadAccess);
}

// IOCTL commands for PMT interface access
constexpr uint32_t ioctlPmtGetTelemetryDiscoverySize = getCtlCode(0x0);
constexpr uint32_t ioctlPmtGetTelemetryDiscovery = getCtlCode(0x1);
constexpr uint32_t ioctlPmtGetTelemetryDword = getCtlCode(0x2);
constexpr uint32_t ioctlPmtGetTelemetryQword = getCtlCode(0x3);

// Data structures used by PMT driver
struct PmtTelemetryEntry {
    unsigned long version;    // Structure version
    unsigned long index;      // Array index within parent structure
    unsigned long guid;       // Globally Unique Id for XML definitions
    unsigned long dWordCount; // Count of DWORDs
};

struct PmtTelemetryDiscovery {
    unsigned long version;                     // Structure version
    unsigned long count;                       // Count of telemetry interfaces
    PmtTelemetryEntry telemetry[anySizeArray]; // Array to hold enries for each interface.
};

struct PmtTelemetryRead {
    unsigned long version; // Structure version
    unsigned long index;   // index of telemetry region returned by GetTelemetryDiscovery
    unsigned long offset;  // Starting DWORD or QWORD index within telemetry region
    unsigned long count;   // Count of DWORD or QWORD to read
};

} // namespace PmtSysman
} // namespace Sysman
} // namespace L0
