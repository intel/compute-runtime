/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/tbx_in_process_simulator.h"

namespace NEO {

const char *const TbxInProcessSimulator::mainFunctionName = "_Z7fs_mainiPPKc";

int *TbxInProcessSimulator::getLibraryLoadFlags() {
    return nullptr;
}

} // namespace NEO
