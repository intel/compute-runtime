/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/drm_helper.h"

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

int DrmHelper::ioctl(Device *device, NEO::DrmIoctl request, void *arg) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    return drm->getIoctlHelper()->ioctl(request, arg);
}

int DrmHelper::getTileIdFromGtId(Device *device, int gtId) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    return drm->getIoctlHelper()->getTileIdFromGtId(gtId);
}

std::string DrmHelper::getSysFsPciPath(Device *device) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    return drm->getSysFsPciPath();
}

int DrmHelper::getErrno(Device *device) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    return drm->getErrno();
}

uint32_t DrmHelper::getEngineTileIndex(Device *device, const NEO::EngineClassInstance &engine) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    auto engineInfo = drm->getEngineInfo();
    return engineInfo->getEngineTileIndex(engine);
}

const NEO::EngineClassInstance *DrmHelper::getEngineInstance(Device *device, uint32_t tile, aub_stream::EngineType engineType) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    auto engineInfo = drm->getEngineInfo();
    return engineInfo->getEngineInstance(tile, engineType);
}

} // namespace L0
