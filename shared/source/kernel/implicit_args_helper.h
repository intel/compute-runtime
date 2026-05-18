/*
 * Copyright (C) 2021-2026 Intel Corporation
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
struct RootDeviceEnvironment;

namespace ImplicitArgsHelper {
std::array<uint8_t, 3> getDimensionOrderForLocalIds(const uint8_t *workgroupDimensionsOrder, std::optional<std::pair<bool /* localIdsGeneratedByRuntime */, uint32_t /* walkOrderForHwGenerationOfLocalIds */>> hwGenerationOfLocalIdsParams);
uint32_t getGrfSize(uint32_t simd);
uint32_t getSizeForImplicitArgsStruct(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, bool localIdsGeneratedByRuntime, const RootDeviceEnvironment &rootDeviceEnvironment);
uint32_t getSizeForImplicitArgsPatching(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, bool localIdsGeneratedByRuntime, const RootDeviceEnvironment &rootDeviceEnvironment);
void *patchImplicitArgs(void *ptrToPatch, const ImplicitArgs &implicitArgs, const KernelDescriptor &kernelDescriptor, std::optional<std::pair<bool /* localIdsGeneratedByRuntime */, uint32_t /* walkOrderForHwGenerationOfLocalIds */>> hwGenerationOfLocalIdsParams, const RootDeviceEnvironment &rootDeviceEnvironment, void **outImplicitArgs);

inline uint32_t resolveImplicitArgsVersion(uint32_t hwDefault, uint32_t binaryVersion) {
    return (binaryVersion > 0) ? binaryVersion : hwDefault;
}

} // namespace ImplicitArgsHelper
} // namespace NEO
