/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <unordered_map>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::NEO::OsAgnosticMemoryManager> : public ::NEO::OsAgnosticMemoryManager {
    using BaseClass = ::NEO::OsAgnosticMemoryManager;
    WhiteBox(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}
};

using MemoryManagerMock = WhiteBox<::NEO::OsAgnosticMemoryManager>;

template <>
struct Mock<NEO::MemoryManager> : public MemoryManagerMock {
    Mock(NEO::ExecutionEnvironment &executionEnvironment);
    MOCK_METHOD2(allocateGraphicsMemoryInPreferredPool,
                 NEO::GraphicsAllocation *(const NEO::AllocationProperties &properties, const void *hostPtr));
    MOCK_METHOD1(freeGraphicsMemory, void(NEO::GraphicsAllocation *));

    NEO::GraphicsAllocation *doAllocateGraphicsMemoryInPreferredPool(const NEO::AllocationProperties &properties, const void *hostPtr);
    void doFreeGraphicsMemory(NEO::GraphicsAllocation *allocation);
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
