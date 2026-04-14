/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
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

    bool operator==(const KmdNotifyProperties &) const = default;
};

namespace KmdNotifyConstants {
inline constexpr int64_t timeoutInMicrosecondsForDisconnectedAcLine = 10000;
inline constexpr uint32_t minimumTaskCountDiffToCheckAcLine = 10;
} // namespace KmdNotifyConstants

} // namespace NEO
