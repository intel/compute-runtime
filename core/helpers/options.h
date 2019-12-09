/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/debug_helpers.h"

#include <cstddef>
#include <cstdint>

#ifndef KMD_PROFILING
#define KMD_PROFILING 0
#endif

enum CommandStreamReceiverType {
    // Use receiver for real HW
    CSR_HW = 0,
    // Capture an AUB file automatically for all traffic going through Device -> CommandStreamReceiver
    CSR_AUB,
    // Capture an AUB and tunnel all commands going through Device -> CommandStreamReceiver to a TBX server
    CSR_TBX,
    // Use receiver for real HW and capture AUB file
    CSR_HW_WITH_AUB,
    // Use TBX server and capture AUB file
    CSR_TBX_WITH_AUB,
    // Number of CSR types
    CSR_TYPES_NUM
};

namespace NEO {
struct HardwareInfo;

// AUB file folder location
extern const char *folderAUB;

// Initial value for HW tag
// Set to 0 if using HW or simulator, otherwise 0xFFFFFF00, needs to be lower then Event::EventNotReady.
extern uint32_t initialHardwareTag;

// Number of devices in the platform
extern size_t numPlatformDevices;
extern const HardwareInfo **platformDevices;
} // namespace NEO
