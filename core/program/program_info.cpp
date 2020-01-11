/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/program/program_info.h"

#include "runtime/program/kernel_info.h"

namespace NEO {

ProgramInfo::~ProgramInfo() {
    for (auto &kernelInfo : kernelInfos) {
        delete kernelInfo;
    }
    kernelInfos.clear();
}

} // namespace NEO
