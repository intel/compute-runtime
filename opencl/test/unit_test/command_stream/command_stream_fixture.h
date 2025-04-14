/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"

#include "opencl/source/command_queue/command_queue.h"

#include <cstdint>

namespace NEO {

struct CommandStreamFixture {
    void setUp(CommandQueue *pCmdQ) {
        pCS = &pCmdQ->getCS(1024);
        pCmdBuffer = pCS->getCpuBase();
    }

    void tearDown() {
    }

    LinearStream *pCS = nullptr;
    void *pCmdBuffer = nullptr;
};
} // namespace NEO
