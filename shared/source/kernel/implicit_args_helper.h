/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "implicit_args.h"

#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace NEO {

struct KernelDescriptor;
class GfxCoreHelper;

inline constexpr const char *implicitArgsRelocationSymbolName = "__INTEL_PATCH_CROSS_THREAD_OFFSET_OFF_R0";

namespace ImplicitArgsHelper {
std::array<uint8_t, 3> getDimensionOrderForLocalIds(const uint8_t *workgroupDimensionsOrder, std::optional<std::pair<bool /* localIdsGeneratedByRuntime */, uint32_t /* walkOrderForHwGenerationOfLocalIds */>> hwGenerationOfLocalIdsParams);
uint32_t getGrfSize(uint32_t simd);
uint32_t getSizeForImplicitArgsPatching(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, bool localIdsGeneratedByRuntime, const GfxCoreHelper &gfxCoreHelper);
void *patchImplicitArgs(void *ptrToPatch, const ImplicitArgs &implicitArgs, const KernelDescriptor &kernelDescriptor, std::optional<std::pair<bool /* localIdsGeneratedByRuntime */, uint32_t /* walkOrderForHwGenerationOfLocalIds */>> hwGenerationOfLocalIdsParams, const GfxCoreHelper &gfxCoreHelper);
} // namespace ImplicitArgsHelper
} // namespace NEO
