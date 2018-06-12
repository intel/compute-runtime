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

#include "hw_cmds.h"
#include "hw_info.h"
#include "runtime/helpers/array_count.h"

namespace OCLRT {
// IP address for TBX server
const char *tbxServerIp = "127.0.0.1";

// AUB file folder location
const char *folderAUB = "aub_out";

// Initial value for HW tag
// Set to 0 if using HW or simulator, otherwise 0xFFFFFF00, needs to be lower then Event::EventNotReady.
uint32_t initialHardwareTag = static_cast<uint32_t>(0);

// Number of devices in the platform
static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_PLATFORM::hwInfo,
};

size_t numPlatformDevices = ARRAY_COUNT(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace OCLRT

bool printMemoryOpCallStack = true;
