/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_time_tbx.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "aubstream/aub_manager.h"

namespace NEO {

DeviceTimeTbx::DeviceTimeTbx(aub_stream::AubManager &aubManager, uint32_t timestampMmioOffset)
    : aubManager(aubManager), lowDwordOffset(timestampMmioOffset), highDwordOffset(timestampMmioOffset + sizeof(uint32_t)) {}

TimeQueryStatus DeviceTimeTbx::getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    pGpuCpuTime->gpuTimeStamp = readSplitTimestamp(*this);
    osTime->getCpuTime(&pGpuCpuTime->cpuTimeinNS);
    return TimeQueryStatus::success;
}

bool DeviceTimeTbx::isTimestampsRefreshEnabled() const {
    return debugManager.flags.EnableReusingGpuTimestamps.getIfNotDefault(false);
}

uint32_t DeviceTimeTbx::readLowDword() const {
    return aubManager.readMMIO(lowDwordOffset);
}

uint32_t DeviceTimeTbx::readHighDword() const {
    return aubManager.readMMIO(highDwordOffset);
}

std::unique_ptr<OSTime> OSTimeTbx::create(aub_stream::AubManager &aubManager, std::optional<uint32_t> timestampMmioOffset) {
    if (!timestampMmioOffset) {
        return nullptr;
    }

    auto deviceTime = std::make_unique<DeviceTimeTbx>(aubManager, *timestampMmioOffset);
    if (!debugManager.flags.EnableTimestampMmioRead.getIfNotDefault(deviceTime->isTimestampMmioReadEnabledByDefault())) {
        return nullptr;
    }

    const auto timestamp = readSplitTimestamp(*deviceTime);

    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "TBX timestamp MMIO 0x%x read: 0x%llx\n",
                 *timestampMmioOffset, static_cast<unsigned long long>(timestamp));

    if (timestamp == 0u) {
        return nullptr;
    }

    auto osTime = std::make_unique<OSTimeTbx>(std::move(deviceTime));
    osTime->setDeviceTimerResolution();
    return osTime;
}

bool OSTimeTbx::getCpuTime(uint64_t *timeStamp) {
    return !getCpuTimeHost(timeStamp);
}

} // namespace NEO
