/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/kmd_notify_properties.h"

#include <atomic>

namespace NEO {
enum QueueThrottle : uint32_t;
using FlushStamp = uint64_t;

class KmdNotifyHelper {
  public:
    KmdNotifyHelper() = delete;
    KmdNotifyHelper(const KmdNotifyProperties *properties) : properties(properties) {};
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
