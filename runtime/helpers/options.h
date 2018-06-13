/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/helpers/debug_helpers.h"
#include <cstdint>
#include <cstddef>

#ifndef OCL_RUNTIME_PROFILING
#define OCL_RUNTIME_PROFILING 0
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

namespace OCLRT {
struct HardwareInfo;

// AUB file folder location
extern const char *folderAUB;

// Initial value for HW tag
// Set to 0 if using HW or simulator, otherwise 0xFFFFFF00, needs to be lower then Event::EventNotReady.
extern uint32_t initialHardwareTag;

// Number of devices in the platform
extern size_t numPlatformDevices;
extern const HardwareInfo **platformDevices;
} // namespace OCLRT
