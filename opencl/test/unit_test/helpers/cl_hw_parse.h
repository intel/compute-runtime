/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/cmd_parse/hw_parse.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

struct ClHardwareParse : HardwareParse {
    using HardwareParse::parseCommands;
    template <typename FamilyType>
    void parseCommands(NEO::CommandQueue &commandQueue) {
        auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();
        auto &commandStream = commandQueue.getCS(1024);

        return HardwareParse::parseCommands<FamilyType>(commandStreamReceiver, commandStream);
    }
};

} // namespace NEO
