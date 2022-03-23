/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/completion_stamp.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace NEO {
struct KmdNotifyProperties {
    int64_t delayKmdNotifyMicroseconds;
    int64_t delayQuickKmdSleepMicroseconds;
    int64_t delayQuickKmdSleepForSporadicWaitsMicroseconds;
    int64_t delayQuickKmdSleepForDirectSubmissionMicroseconds;
    // Main switch for KMD Notify optimization - if its disabled, all below are disabled too
    bool enableKmdNotify;
    // Use smaller delay in specific situations (ie. from AsyncEventsHandler)
    bool enableQuickKmdSleep;
    // If waits are called sporadically  use QuickKmdSleep mode, otherwise use standard delay
    bool enableQuickKmdSleepForSporadicWaits;
    // If direct submission is enabled, use direct submission delay, otherwise use standard delay
    bool enableQuickKmdSleepForDirectSubmission;
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

    WaitParams obtainTimeoutParams(bool quickKmdSleepRequest,
                                   uint32_t currentHwTag,
                                   uint32_t taskCountToWait,
                                   FlushStamp flushStampToWait,
                                   QueueThrottle throttle,
                                   bool kmdWaitModeActive,
                                   bool directSubmissionEnabled);

    bool quickKmdSleepForSporadicWaitsEnabled() const { return properties->enableQuickKmdSleepForSporadicWaits; }
    MOCKABLE_VIRTUAL void updateLastWaitForCompletionTimestamp();
    MOCKABLE_VIRTUAL void updateAcLineStatus();

    static void overrideFromDebugVariable(int32_t debugVariableValue, int64_t &destination);
    static void overrideFromDebugVariable(int32_t debugVariableValue, bool &destination);

  protected:
    bool applyQuickKmdSleepForSporadicWait() const;
    int64_t getBaseTimeout(const int64_t &multiplier) const;
    int64_t getMicrosecondsSinceEpoch() const;

    const KmdNotifyProperties *properties = nullptr;
    std::atomic<int64_t> lastWaitForCompletionTimestampUs{0};
    std::atomic<bool> acLineConnected{true};
};
} // namespace NEO
