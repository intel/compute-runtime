/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman.h"

#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include <vector>

namespace L0 {

static SysmanHandleContext *SysmanHandleContextInstance = nullptr;

void SysmanHandleContext::init(ze_init_flag_t flag) {
    if (SysmanHandleContextInstance == nullptr) {
        SysmanHandleContextInstance = new SysmanHandleContext();
    }
}

SysmanHandleContext::SysmanHandleContext() {
    DriverHandle *dH = L0::DriverHandle::fromHandle(GlobalDriver.get());
    uint32_t count = 0;
    dH->getDevice(&count, nullptr);
    std::vector<ze_device_handle_t> devices(count);
    dH->getDevice(&count, devices.data());

    for (auto device : devices) {
        SysmanImp *sysman = new SysmanImp(device);
        UNRECOVERABLE_IF(!sysman);
        sysman->init();
        handle_map[device] = sysman;
    }
}

ze_result_t SysmanHandleContext::sysmanGet(zet_device_handle_t hDevice, zet_sysman_handle_t *phSysman) {

    if (SysmanHandleContextInstance == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    auto got = SysmanHandleContextInstance->handle_map.find(hDevice);
    if (got == SysmanHandleContextInstance->handle_map.end()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *phSysman = got->second;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
