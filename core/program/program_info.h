/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/compiler_interface/linker.h"
#include "core/utilities/stackvec.h"

#include <memory>
#include <vector>

namespace NEO {
struct KernelInfo;

struct ProgramInfo {
    ProgramInfo() = default;
    ProgramInfo(ProgramInfo &&) = default;
    ProgramInfo &operator=(ProgramInfo &&) = default;
    ~ProgramInfo();

    struct GlobalSurfaceInfo {
        const void *initData = nullptr;
        size_t size = 0U;
    };

    void prepareLinkerInputStorage() {
        if (this->linkerInput == nullptr) {
            this->linkerInput = std::make_unique<LinkerInput>();
        }
    }

    GlobalSurfaceInfo globalConstants;
    GlobalSurfaceInfo globalVariables;
    std::unique_ptr<LinkerInput> linkerInput;

    std::vector<KernelInfo *> kernelInfos;
};

} // namespace NEO
