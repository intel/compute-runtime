/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

void DeviceImp::processAdditionalKernelProperties(const NEO::GfxCoreHelper &gfxCoreHelper, ze_device_module_properties_t *pKernelProperties) {
}

DeviceImp::CmdListCreateFunPtrT DeviceImp::getCmdListCreateFunc(const ze_command_list_desc_t *desc) {
    return &CommandList::create;
}

uint32_t DeviceImp::getAdditionalEngines(uint32_t numAdditionalEnginesRequested,
                                         ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    return 0;
}
} // namespace L0
