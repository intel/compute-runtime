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

bool KmdNotifyProperties::timeoutEnabled(FlushStamp flushStampToWait) const {
    return enableKmdNotify && flushStampToWait != 0;
}

int64_t KmdNotifyProperties::pickTimeoutValue(std::chrono::high_resolution_clock::time_point &lastWaitTimestamp,
                                              bool quickKmdSleepRequest, uint32_t currentHwTag, uint32_t taskCountToWait) const {
    quickKmdSleepRequest |= applyQuickKmdSleepForSporadicWait(lastWaitTimestamp);

    if (quickKmdSleepRequest && enableQuickKmdSleep) {
        return delayQuickKmdSleepMicroseconds;
    }

    int64_t multiplier = (currentHwTag < taskCountToWait) ? static_cast<int64_t>(taskCountToWait - currentHwTag) : 1;

    return delayKmdNotifyMicroseconds * multiplier;
}

bool KmdNotifyProperties::applyQuickKmdSleepForSporadicWait(std::chrono::high_resolution_clock::time_point &lastWaitTimestamp) const {
    if (enableQuickKmdSleepForSporadicWaits) {
        auto now = std::chrono::high_resolution_clock::now();
        auto timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(now - lastWaitTimestamp).count();
        if (timeDiff > delayQuickKmdSleepForSporadicWaitsMicroseconds) {
            return true;
        }
    }
    return false;
}

void KmdNotifyProperties::overrideFromDebugVariable(int32_t debugVariableValue, int64_t &destination) {
    if (debugVariableValue >= 0) {
        destination = static_cast<int64_t>(debugVariableValue);
    }
}

void KmdNotifyProperties::overrideFromDebugVariable(int32_t debugVariableValue, bool &destination) {
    if (debugVariableValue >= 0) {
        destination = !!(debugVariableValue);
    }
}
