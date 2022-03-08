/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/linker.h"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace NEO {
struct KernelInfo;

struct ProgramInfo {
    ProgramInfo() = default;
    ProgramInfo(ProgramInfo &&) = default;
    ProgramInfo &operator=(ProgramInfo &&) = default;
    ProgramInfo(const ProgramInfo &) = delete;
    ProgramInfo &operator=(const ProgramInfo &) = delete;
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
    GlobalSurfaceInfo globalStrings;
    std::unique_ptr<LinkerInput> linkerInput;
    std::unordered_map<std::string, std::string> globalsDeviceToHostNameMap;

    std::vector<ExternalFunctionInfo> externalFunctions;
    std::vector<KernelInfo *> kernelInfos;
    Elf::Elf<Elf::EI_CLASS_64> decodedElf;
    uint32_t grfSize = 32U;
};

size_t getMaxInlineSlmNeeded(const ProgramInfo &programInfo);
bool requiresLocalMemoryWindowVA(const ProgramInfo &programInfo);

} // namespace NEO
