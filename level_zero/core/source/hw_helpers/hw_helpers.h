/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"

#include <cstdint>

namespace L0 {
inline uint64_t getIntermediateCacheSize(const NEO::HardwareInfo &hwInfo) {
    return 0u;
}

inline void waitForTaskCountWithKmdNotifyFallbackHelper(NEO::CommandStreamReceiver *csr,
                                                        uint32_t taskCountToWait,
                                                        NEO::FlushStamp flushStampToWait,
                                                        bool useQuickKmdSleep,
                                                        bool forcePowerSavingMode) {
}

} // namespace L0
