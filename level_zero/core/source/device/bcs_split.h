/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/sku_info/sku_info_base.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"

#include <vector>

namespace NEO {
class CommandStreamReceiver;
}

namespace L0 {
struct CommandQueue;
struct DeviceImp;

struct BcsSplit {
    DeviceImp &device;

    std::vector<CommandQueue *> cmdQs;
    NEO::BcsInfoMask engines = NEO::EngineHelpers::oddLinkedCopyEnginesMask;

    void setupDevice(uint32_t productFamily, bool internalUsage, const ze_command_queue_desc_t *desc, NEO::CommandStreamReceiver *csr);
    void releaseResources();

    BcsSplit(DeviceImp &device) : device(device){};
};

} // namespace L0