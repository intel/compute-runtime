/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>

#include <cstdint>
#include <variant>
#include <vector>

#ifndef BIT
#define BIT(x) (1u << (x))
#endif

namespace L0::MCL {

using MutableFlags = uint32_t;
struct MutableFlag {
    static constexpr MutableFlags kernelArgument = BIT(0);
    static constexpr MutableFlags groupCount = BIT(1);
    static constexpr MutableFlags groupSize = BIT(2);
    static constexpr MutableFlags globalOffset = BIT(3);
    static constexpr MutableFlags signalEvent = BIT(4);
    static constexpr MutableFlags waitEvents = BIT(5);
    static constexpr MutableFlags kernelInstruction = BIT(6);
    static constexpr MutableFlags all = kernelArgument | groupCount | groupSize | globalOffset |
                                        signalEvent | waitEvents | kernelInstruction;
};

struct MutableCommandIdDescriptor {
    MutableFlags flags = 0;
};

struct KernelArgumentMutationDescriptor {
    uint64_t commandId = 0;
    uint32_t argIndex = 0;
    size_t argSize = 0;
    const void *argValue = nullptr;
};

struct GroupCountMutationDescriptor {
    uint64_t commandId = 0;
    ze_group_count_t groupCount = {};
};

struct GroupSizeMutationDescriptor {
    uint64_t commandId = 0;
    uint32_t groupSize[3] = {0, 0, 0};
};

struct GlobalOffsetMutationDescriptor {
    uint64_t commandId = 0;
    uint32_t globalOffset[3] = {0, 0, 0};
};

using MutationDescriptor = std::variant<KernelArgumentMutationDescriptor, GroupCountMutationDescriptor, GroupSizeMutationDescriptor, GlobalOffsetMutationDescriptor>;
using MutationDescriptorContainer = std::vector<MutationDescriptor>;

} // namespace L0::MCL
