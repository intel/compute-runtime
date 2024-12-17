/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/vf_management/sysman_vf_management.h"

#include "level_zero/sysman/source/api/vf_management/sysman_os_vf.h"
#include "level_zero/sysman/source/api/vf_management/sysman_vf_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"

#include <iostream>

namespace L0 {
namespace Sysman {

VfManagementHandleContext::~VfManagementHandleContext() {
    handleList.clear();
}

void VfManagementHandleContext::createHandle(uint32_t vfId) {
    std::unique_ptr<VfManagement> pVf = std::make_unique<VfImp>(pOsSysman, vfId);
    handleList.push_back(std::move(pVf));
}

ze_result_t VfManagementHandleContext::init() {
    uint32_t enabledVfNum = OsVf::getNumEnabledVfs(pOsSysman);
    for (uint32_t vfIndex = 1; vfIndex <= enabledVfNum; vfIndex++) {
        createHandle(vfIndex);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t VfManagementHandleContext::vfManagementGet(uint32_t *pCount, zes_vf_handle_t *phVfManagement) {
    std::call_once(initVfManagementOnce, [this]() {
        this->init();
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
        return ZE_RESULT_SUCCESS;
    }
    if (nullptr != phVfManagement) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phVfManagement[i] = handleList[i]->toVfManagementHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
