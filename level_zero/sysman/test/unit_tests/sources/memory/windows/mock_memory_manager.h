/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include <unordered_map>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace Sysman {
namespace ult {

template <typename Type>
struct WhiteBox : public Type {
    using Type::Type;
};

template <typename Type>
WhiteBox<Type> *whiteboxCast(Type *obj) {
    return static_cast<WhiteBox<Type> *>(obj);
}

template <typename Type>
WhiteBox<Type> &whiteboxCast(Type &obj) {
    return static_cast<WhiteBox<Type> &>(obj);
}

template <typename Type>
Type *blackboxCast(WhiteBox<Type> *obj) {
    return static_cast<Type *>(obj);
}

template <>
struct WhiteBox<::NEO::OsAgnosticMemoryManager> : public ::NEO::OsAgnosticMemoryManager {
    using BaseClass = ::NEO::OsAgnosticMemoryManager;
    using BaseClass::localMemorySupported;
    WhiteBox(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}
};

using MemoryManagerMock = WhiteBox<::NEO::OsAgnosticMemoryManager>;

} // namespace ult
} // namespace Sysman
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
