/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "implicit_args.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace NEO {

struct KernelDescriptor;
struct RootDeviceEnvironment;

inline constexpr std::string_view implicitArgsRelocationSymbolName = "__INTEL_PATCH_CROSS_THREAD_OFFSET_OFF_R0";

namespace ImplicitArgsHelper {
std::array<uint8_t, 3> getDimensionOrderForLocalIds(const uint8_t *workgroupDimensionsOrder, std::optional<std::pair<bool /* localIdsGeneratedByRuntime */, uint32_t /* walkOrderForHwGenerationOfLocalIds */>> hwGenerationOfLocalIdsParams);
uint32_t getGrfSize(uint32_t simd);
uint32_t getSizeForImplicitArgsStruct(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, bool localIdsGeneratedByRuntime, const RootDeviceEnvironment &rootDeviceEnvironment);
uint32_t getSizeForImplicitArgsPatching(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, bool localIdsGeneratedByRuntime, const RootDeviceEnvironment &rootDeviceEnvironment);
void *patchImplicitArgs(void *ptrToPatch, const ImplicitArgs &implicitArgs, const KernelDescriptor &kernelDescriptor, std::optional<std::pair<bool /* localIdsGeneratedByRuntime */, uint32_t /* walkOrderForHwGenerationOfLocalIds */>> hwGenerationOfLocalIdsParams, const RootDeviceEnvironment &rootDeviceEnvironment, void **outImplicitArgs);
} // namespace ImplicitArgsHelper
} // namespace NEO
