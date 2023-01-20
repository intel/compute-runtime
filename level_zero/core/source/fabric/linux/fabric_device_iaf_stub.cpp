/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fabric/linux/fabric_device_iaf_stub.h"

#include "level_zero/core/source/fabric/fabric.h"

namespace L0 {

std::unique_ptr<FabricDeviceInterface> FabricDeviceInterface::createFabricDeviceInterfaceIaf(const FabricVertex *fabricVertex) {

    return std::unique_ptr<FabricDeviceInterface>(new (std::nothrow) FabricDeviceIafStub());
}

} // namespace L0
