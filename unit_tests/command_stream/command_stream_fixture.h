/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/command_queue/command_queue.h"

#include <cstdint>

namespace NEO {

struct CommandStreamFixture {
    CommandStreamFixture(void)
        : pCS(nullptr),
          pCmdBuffer(nullptr) {
    }

    void SetUp(CommandQueue *pCmdQ) {
        pCS = &pCmdQ->getCS(1024);
        pCmdBuffer = pCS->getCpuBase();
    }

    virtual void TearDown(void) {
    }

    LinearStream *pCS;
    void *pCmdBuffer;
};
} // namespace NEO
