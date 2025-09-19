/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <initguid.h>

namespace L0 {
namespace Sysman {
namespace PmtSysman {

constexpr uint32_t FileDeviceIntelPmt = 0x9998;
constexpr uint32_t PmtMaxInterfaces = 2;
constexpr uint32_t AnySizeArray = 1;
constexpr uint32_t MethodBuffered = 0;
constexpr uint32_t FileReadAccess = 0x0001;

// Intel PMT Telemetry Interface GUID {3dfb2563-5c44-4c59-8d80-baea7d06e6b8}
constexpr GUID GuidInterfacePmtTelemetry = {0x3dfb2563, 0x5c44, 0x4c59, {0x8d, 0x80, 0xba, 0xea, 0x7d, 0x06, 0xe6, 0xb8}};

inline constexpr uint32_t getCtlCode(uint32_t code) {
    return CTL_CODE(FileDeviceIntelPmt, code, MethodBuffered, FileReadAccess);
}

// IOCTL commands for PMT interface access
constexpr uint32_t IoctlPmtGetTelemetryDiscoverySize = getCtlCode(0x0);
constexpr uint32_t IoctlPmtGetTelemetryDiscovery = getCtlCode(0x1);
constexpr uint32_t IoctlPmtGetTelemtryDword = getCtlCode(0x2);
constexpr uint32_t IoctlPmtGetTelemtryQword = getCtlCode(0x3);

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
    PmtTelemetryEntry telemetry[AnySizeArray]; // Array to hold enries for each interface.
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