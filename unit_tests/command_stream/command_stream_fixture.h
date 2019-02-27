/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue.h"

#include <cstdint>

namespace OCLRT {

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
} // namespace OCLRT
