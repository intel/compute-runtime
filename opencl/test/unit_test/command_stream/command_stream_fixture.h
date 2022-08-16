/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
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
