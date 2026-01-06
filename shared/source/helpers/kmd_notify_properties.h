/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/command_stream/wait_status.h"

#include <atomic>
#include <cstdint>

namespace NEO {
enum QueueThrottle : uint32_t;
using FlushStamp = uint64_t;

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

    bool operator==(const KmdNotifyProperties &) const = default;
};

namespace KmdNotifyConstants {
inline constexpr int64_t timeoutInMicrosecondsForDisconnectedAcLine = 10000;
inline constexpr uint32_t minimumTaskCountDiffToCheckAcLine = 10;
} // namespace KmdNotifyConstants

class KmdNotifyHelper {
  public:
    KmdNotifyHelper() = delete;
    KmdNotifyHelper(const KmdNotifyProperties *properties) : properties(properties){};
    MOCKABLE_VIRTUAL ~KmdNotifyHelper() = default;

    WaitParams obtainTimeoutParams(bool quickKmdSleepRequest,
                                   TagAddressType currentHwTag,
                                   TaskCountType taskCountToWait,
                                   FlushStamp flushStampToWait,
                                   QueueThrottle throttle,
                                   bool kmdWaitModeActive,
                                   bool directSubmissionEnabled);

    bool quickKmdSleepForSporadicWaitsEnabled() const { return properties->enableQuickKmdSleepForSporadicWaits; }
    MOCKABLE_VIRTUAL void updateLastWaitForCompletionTimestamp();
    MOCKABLE_VIRTUAL void updateAcLineStatus();
    bool getAcLineConnected() const { return acLineConnected.load(); }

    static void overrideFromDebugVariable(int32_t debugVariableValue, int64_t &destination);
    static void overrideFromDebugVariable(int32_t debugVariableValue, bool &destination);

  protected:
    bool applyQuickKmdSleepForSporadicWait() const;
    int64_t getMicrosecondsSinceEpoch() const;

    const KmdNotifyProperties *properties = nullptr;
    std::atomic<int64_t> lastWaitForCompletionTimestampUs{0};
    std::atomic<bool> acLineConnected{true};
};
} // namespace NEO
