/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include <level_zero/ze_api.h>

namespace NEO {
class MockDevice;
class MockMemoryOperationsHandlerTests;
} // namespace NEO
namespace L0 {
struct Context;
struct Device;
struct DriverHandleImp;
class HostPointerManager;

namespace ult {
template <typename Type>
struct WhiteBox;

struct HostPointerManagerFixure {
    void setUp();
    void tearDown();

    DebugManagerStateRestore debugRestore;
    std::unique_ptr<WhiteBox<L0::DriverHandleImp>> hostDriverHandle;

    WhiteBox<L0::HostPointerManager> *openHostPointerManager = nullptr;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    NEO::MockMemoryOperationsHandlerTests *mockMemoryInterface = nullptr;
    ze_context_handle_t hContext;
    L0::Context *context = nullptr;

    void *heapPointer = nullptr;
    size_t heapSize = 4 * MemoryConstants::pageSize;
};

struct ForceDisabledHostPointerManagerFixure : public HostPointerManagerFixure {
    void setUp() {
        debugManager.flags.EnableHostPointerImport.set(0);

        HostPointerManagerFixure::setUp();
    }

    void tearDown() {
        HostPointerManagerFixure::tearDown();
    }
};

struct ForceEnabledHostPointerManagerFixure : public HostPointerManagerFixure {
    void setUp() {
        debugManager.flags.EnableHostPointerImport.set(1);

        HostPointerManagerFixure::setUp();
    }

    void tearDown() {
        HostPointerManagerFixure::tearDown();
    }
};

} // namespace ult
} // namespace L0
