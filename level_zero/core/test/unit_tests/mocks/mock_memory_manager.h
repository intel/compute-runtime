/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

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
    using BaseClass::localMemorySupported;
    WhiteBox(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}
};

using MemoryManagerMock = WhiteBox<::NEO::OsAgnosticMemoryManager>;

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
