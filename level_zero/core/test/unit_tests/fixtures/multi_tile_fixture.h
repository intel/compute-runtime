/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

namespace L0 {
struct ContextImp;
struct Device;
struct CommandListImp;

namespace ult {
template <typename Type>
struct WhiteBox;

struct MultiTileCommandListAppendLaunchKernelFixture : public MultiDeviceModuleFixture {
    MultiTileCommandListAppendLaunchKernelFixture();
    void setUp();
    void tearDown();

    ContextImp *contextImp = nullptr;
    WhiteBox<::L0::CommandListImp> *commandList = nullptr;
    L0::Device *device = nullptr;
    VariableBackup<bool> backup;
};

struct MultiTileImmediateCommandListAppendLaunchKernelFixture : public MultiDeviceModuleFixture {
    MultiTileImmediateCommandListAppendLaunchKernelFixture();
    void setUp();
    void tearDown();

    ContextImp *contextImp = nullptr;
    L0::Device *device = nullptr;
    VariableBackup<bool> backupApiSupport;
    VariableBackup<bool> backupLocalMemory;
};

} // namespace ult
} // namespace L0
