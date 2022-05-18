/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

void DeviceImp::processAdditionalKernelProperties(NEO::HwHelper &hwHelper, ze_device_module_properties_t *pKernelProperties) {
}

DeviceImp::CmdListCreateFunPtrT DeviceImp::getCmdListCreateFunc(const ze_command_list_desc_t *desc) {
    return &CommandList::create;
}
} // namespace L0
