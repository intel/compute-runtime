/*
 * Copyright (c) 2018, Intel Corporation
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
#include "runtime/helpers/completion_stamp.h"

#include <cstdint>
#include <chrono>

namespace OCLRT {
struct KmdNotifyProperties {
    // Main switch for KMD Notify optimization - if its disabled, all below are disabled too
    bool enableKmdNotify;
    int64_t delayKmdNotifyMicroseconds;
    // Use smaller delay in specific situations (ie. from AsyncEventsHandler)
    bool enableQuickKmdSleep;
    int64_t delayQuickKmdSleepMicroseconds;
    // If waits are called sporadically  use QuickKmdSleep mode, otherwise use standard delay
    bool enableQuickKmdSleepForSporadicWaits;
    int64_t delayQuickKmdSleepForSporadicWaitsMicroseconds;

    bool timeoutEnabled(FlushStamp flushStampToWait) const;

    int64_t pickTimeoutValue(std::chrono::high_resolution_clock::time_point &lastWaitTimestamp,
                             bool quickKmdSleepRequest, uint32_t currentHwTag, uint32_t taskCountToWait) const;

    bool applyQuickKmdSleepForSporadicWait(std::chrono::high_resolution_clock::time_point &lastWaitTimestamp) const;

    static void overrideFromDebugVariable(int32_t debugVariableValue, int64_t &destination);
    static void overrideFromDebugVariable(int32_t debugVariableValue, bool &destination);
};
} // namespace OCLRT
