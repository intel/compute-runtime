/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/arrayref.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
class Device;
struct ExternalFunctionInfo;
struct LinkerInput;
struct KernelInfo;

struct ProgramInfo : NEO::NonCopyableClass {
    ProgramInfo() = default;
    ProgramInfo(ProgramInfo &&) = default;
    ProgramInfo &operator=(ProgramInfo &&) = default;
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
    std::vector<std::string> requiredLibs;
    uint32_t grfSize = 32U;
    uint32_t minScratchSpaceSize = 0U;
    uint32_t indirectDetectionVersion = 0U;
    uint32_t indirectAccessBufferMajorVersion = 0U;
    size_t kernelMiscInfoPos = std::string::npos;
    uint32_t samplerStateSize = 0u;
    uint32_t samplerBorderColorStateSize = 0u;
};

static_assert(NEO::NonCopyable<ProgramInfo>);

size_t getMaxInlineSlmNeeded(const ProgramInfo &programInfo);
bool requiresLocalMemoryWindowVA(const ProgramInfo &programInfo);

} // namespace NEO
