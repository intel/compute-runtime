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

#include <cstdint>
#include "runtime/helpers/kmd_notify_properties.h"

using namespace OCLRT;

bool KmdNotifyHelper::obtainTimeoutParams(int64_t &timeoutValueOutput,
                                          bool quickKmdSleepRequest,
                                          uint32_t currentHwTag,
                                          uint32_t taskCountToWait,
                                          FlushStamp flushStampToWait) {
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

    return flushStampToWait != 0 && (properties->enableKmdNotify || !acLineConnected);
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
