/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/fabric/fabric.h"

namespace L0 {

void FabricVertex::discoverFabricVertices(std::vector<Device *> &devices, std::vector<FabricVertex *> &fabricVertices) {
    for (auto &device : devices) {
        auto fabricVertex = FabricVertex::createFromDevice(device);
        device->setFabricVertex(fabricVertex);
        fabricVertices.push_back(fabricVertex);
    }
}

} // namespace L0
