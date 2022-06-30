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
    void SetUp(CommandQueue *pCmdQ) { // NOLINT(readability-identifier-naming)
        pCS = &pCmdQ->getCS(1024);
        pCmdBuffer = pCS->getCpuBase();
    }

    virtual void TearDown() { // NOLINT(readability-identifier-naming)
    }

    LinearStream *pCS = nullptr;
    void *pCmdBuffer = nullptr;
};
} // namespace NEO
