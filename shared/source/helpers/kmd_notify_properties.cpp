/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <cstdint>

using namespace NEO;

bool KmdNotifyHelper::obtainTimeoutParams(int64_t &timeoutValueOutput,
                                          bool quickKmdSleepRequest,
                                          uint32_t currentHwTag,
                                          uint32_t taskCountToWait,
                                          FlushStamp flushStampToWait,
                                          bool forcePowerSavingMode) {
    if (flushStampToWait == 0) {
        return false;
    }

    if (DebugManager.flags.PowerSavingMode.get() || forcePowerSavingMode) {
        timeoutValueOutput = 1;
        return true;
    }

    int64_t multiplier = (currentHwTag < taskCountToWait) ? static_cast<int64_t>(taskCountToWait - currentHwTag) : 1;
    if (!properties->enableKmdNotify && multiplier > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine) {
        updateAcLineStatus();
    }

    quickKmdSleepRequest |= applyQuickKmdSleepForSporadicWait();

    if (!properties->enableKmdNotify && !acLineConnected) {
        timeoutValueOutput = KmdNotifyConstants::timeoutInMicrosecondsForDisconnectedAcLine;
    } else if (quickKmdSleepRequest && properties->enableQuickKmdSleep) {
        timeoutValueOutput = properties->delayQuickKmdSleepMicroseconds;
    } else {
        timeoutValueOutput = getBaseTimeout(multiplier);
    }

    return (properties->enableKmdNotify || !acLineConnected);
}

bool KmdNotifyHelper::applyQuickKmdSleepForSporadicWait() const {
    if (properties->enableQuickKmdSleepForSporadicWaits) {
        auto timeDiff = getMicrosecondsSinceEpoch() - lastWaitForCompletionTimestampUs.load();
        if (timeDiff > properties->delayQuickKmdSleepForSporadicWaitsMicroseconds) {
            return true;
        }
    }
    return false;
}

void KmdNotifyHelper::updateLastWaitForCompletionTimestamp() {
    lastWaitForCompletionTimestampUs = getMicrosecondsSinceEpoch();
}

int64_t KmdNotifyHelper::getMicrosecondsSinceEpoch() const {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

void KmdNotifyHelper::overrideFromDebugVariable(int32_t debugVariableValue, int64_t &destination) {
    if (debugVariableValue >= 0) {
        destination = static_cast<int64_t>(debugVariableValue);
    }
}

void KmdNotifyHelper::overrideFromDebugVariable(int32_t debugVariableValue, bool &destination) {
    if (debugVariableValue >= 0) {
        destination = !!(debugVariableValue);
    }
}
