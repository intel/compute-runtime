/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/arrayref.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
class Device;
struct ExternalFunctionInfo;
struct LinkerInput;
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
        size_t zeroInitSize = 0U;
    };

    void prepareLinkerInputStorage();

    GlobalSurfaceInfo globalConstants;
    GlobalSurfaceInfo globalVariables;
    GlobalSurfaceInfo globalStrings;
    std::unique_ptr<LinkerInput> linkerInput;
    std::unordered_map<std::string, std::string> globalsDeviceToHostNameMap;

    std::vector<ExternalFunctionInfo> externalFunctions;
    std::vector<KernelInfo *> kernelInfos;
    uint32_t grfSize = 32U;
    uint32_t minScratchSpaceSize = 0U;
    uint32_t indirectDetectionVersion = 0U;
    size_t kernelMiscInfoPos = std::string::npos;
};

size_t getMaxInlineSlmNeeded(const ProgramInfo &programInfo);
bool requiresLocalMemoryWindowVA(const ProgramInfo &programInfo);
bool isRebuiltToPatchtokensRequired(Device *neoDevice, ArrayRef<const uint8_t> archive, std::string &optionsString, bool isBuiltin, bool isVmeUsed);

} // namespace NEO
