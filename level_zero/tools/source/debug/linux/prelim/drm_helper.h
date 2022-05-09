/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/topology_map.h"

#include "level_zero/core/source/device/device.h"

namespace NEO {
struct EngineClassInstance;
}

namespace L0 {
struct DrmHelper {
    static int ioctl(Device *device, unsigned long request, void *arg);
    static int getErrno(Device *device);
    static uint32_t getEngineTileIndex(Device *device, const NEO::EngineClassInstance &engine);
    static const NEO::EngineClassInstance *getEngineInstance(Device *device, uint32_t tile, aub_stream::EngineType engineType);
    static const NEO::TopologyMap &getTopologyMap(Device *device);
};

} // namespace L0
