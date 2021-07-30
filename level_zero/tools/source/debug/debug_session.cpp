/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

#include "shared/source/helpers/hw_info.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

ze_device_thread_t DebugSession::convertToPhysical(ze_device_thread_t thread, uint32_t &deviceIndex) {
    auto hwInfo = connectedDevice->getHwInfo();
    auto deviceBitfield = connectedDevice->getNEODevice()->getDeviceBitfield();

    if (connectedDevice->getNEODevice()->isSubDevice()) {
        deviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    } else if (thread.slice != UINT32_MAX) {
        deviceIndex = thread.slice / hwInfo.gtSystemInfo.SliceCount;
        thread.slice = thread.slice % hwInfo.gtSystemInfo.SliceCount;
    }

    return thread;
}

EuThread::ThreadId DebugSession::convertToThreadId(ze_device_thread_t thread) {
    auto hwInfo = connectedDevice->getHwInfo();
    auto deviceBitfield = connectedDevice->getNEODevice()->getDeviceBitfield();

    UNRECOVERABLE_IF(!DebugSession::isSingleThread(thread));

    uint32_t deviceIndex = 0;
    if (connectedDevice->getNEODevice()->isSubDevice()) {
        deviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    } else {
        deviceIndex = thread.slice / hwInfo.gtSystemInfo.SliceCount;
        thread.slice = thread.slice % hwInfo.gtSystemInfo.SliceCount;
    }

    EuThread::ThreadId threadId(deviceIndex, thread.slice, thread.subslice, thread.eu, thread.thread);
    return threadId;
}

ze_device_thread_t DebugSession::convertToApi(EuThread::ThreadId threadId) {
    auto hwInfo = connectedDevice->getHwInfo();

    ze_device_thread_t thread = {static_cast<uint32_t>(threadId.slice), static_cast<uint32_t>(threadId.subslice), static_cast<uint32_t>(threadId.eu), static_cast<uint32_t>(threadId.thread)};

    if (!connectedDevice->getNEODevice()->isSubDevice()) {
        thread.slice = thread.slice + static_cast<uint32_t>(threadId.tileIndex * hwInfo.gtSystemInfo.SliceCount);
    }
    return thread;
}

DebugSession::DebugSession(const zet_debug_config_t &config, Device *device) : connectedDevice(device) {

    if (connectedDevice) {
        auto hwInfo = connectedDevice->getHwInfo();
        const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
        const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
        const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
        uint32_t subDeviceCount = 1;

        for (uint32_t tileIndex = 0; tileIndex < subDeviceCount; tileIndex++) {
            for (uint32_t sliceID = 0; sliceID < hwInfo.gtSystemInfo.MaxSlicesSupported; sliceID++) {
                for (uint32_t subsliceID = 0; subsliceID < numSubslicesPerSlice; subsliceID++) {
                    for (uint32_t euID = 0; euID < numEuPerSubslice; euID++) {

                        for (uint32_t threadID = 0; threadID < numThreadsPerEu; threadID++) {

                            EuThread::ThreadId thread = {tileIndex, sliceID, subsliceID, euID, threadID};

                            allThreads[uint64_t(thread)] = std::make_unique<EuThread>(thread);
                        }
                    }
                }
            }
        }
    }
}

std::vector<ze_device_thread_t> DebugSession::getSingleThreads(ze_device_thread_t physicalThread, const NEO::HardwareInfo &hwInfo) {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

    UNRECOVERABLE_IF(numThreadsPerEu > 8);

    std::vector<ze_device_thread_t> threads;

    const uint32_t slice = physicalThread.slice;
    const uint32_t subslice = physicalThread.subslice;
    const uint32_t eu = physicalThread.eu;
    const uint32_t thread = physicalThread.thread;

    for (uint32_t sliceID = 0; sliceID < hwInfo.gtSystemInfo.MaxSlicesSupported; sliceID++) {
        if (slice != UINT32_MAX) {
            sliceID = slice;
        }

        for (uint32_t subsliceID = 0; subsliceID < numSubslicesPerSlice; subsliceID++) {
            if (subslice != UINT32_MAX) {
                subsliceID = subslice;
            }

            for (uint32_t euID = 0; euID < numEuPerSubslice; euID++) {
                if (eu != UINT32_MAX) {
                    euID = eu;
                }

                for (uint32_t threadID = 0; threadID < numThreadsPerEu; threadID++) {
                    if (thread != UINT32_MAX) {
                        threadID = thread;
                    }
                    threads.push_back({sliceID, subsliceID, euID, threadID});

                    if (thread != UINT32_MAX) {
                        break;
                    }
                }

                if (eu != UINT32_MAX) {
                    break;
                }
            }
            if (subslice != UINT32_MAX) {
                break;
            }
        }
        if (slice != UINT32_MAX) {
            break;
        }
    }

    return threads;
}

bool DebugSession::isBindlessSystemRoutine() {
    if (debugArea.reserved1 &= 1) {
        return true;
    }
    return false;
}

} // namespace L0
