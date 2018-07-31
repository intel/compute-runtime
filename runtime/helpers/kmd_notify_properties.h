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
#include <atomic>

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
};

namespace KmdNotifyConstants {
constexpr int64_t timeoutInMicrosecondsForDisconnectedAcLine = 10000;
constexpr uint32_t minimumTaskCountDiffToCheckAcLine = 10;
} // namespace KmdNotifyConstants

class KmdNotifyHelper {
  public:
    KmdNotifyHelper() = delete;
    KmdNotifyHelper(const KmdNotifyProperties *properties) : properties(properties){};
    MOCKABLE_VIRTUAL ~KmdNotifyHelper() = default;

    bool obtainTimeoutParams(int64_t &timeoutValueOutput,
                             bool quickKmdSleepRequest,
                             uint32_t currentHwTag,
                             uint32_t taskCountToWait,
                             FlushStamp flushStampToWait);

    bool quickKmdSleepForSporadicWaitsEnabled() const { return properties->enableQuickKmdSleepForSporadicWaits; }
    MOCKABLE_VIRTUAL void updateLastWaitForCompletionTimestamp();
    MOCKABLE_VIRTUAL void updateAcLineStatus();

    static void overrideFromDebugVariable(int32_t debugVariableValue, int64_t &destination);
    static void overrideFromDebugVariable(int32_t debugVariableValue, bool &destination);

    void initMaxPowerSavingMode() {
        maxPowerSavingMode = true;
    }

  protected:
    bool applyQuickKmdSleepForSporadicWait() const;
    int64_t getBaseTimeout(const int64_t &multiplier) const;
    int64_t getMicrosecondsSinceEpoch() const;

    const KmdNotifyProperties *properties = nullptr;
    std::atomic<int64_t> lastWaitForCompletionTimestampUs{0};
    std::atomic<bool> acLineConnected{true};
    bool maxPowerSavingMode = false;
};
} // namespace OCLRT
