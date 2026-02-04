/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

namespace L0 {
struct ContextImp;
struct Device;
struct CommandList;

namespace ult {
template <typename Type>
struct WhiteBox;

struct MultiTileCommandListAppendLaunchKernelFixture : public MultiDeviceModuleFixture {
    MultiTileCommandListAppendLaunchKernelFixture();
    void setUp();
    void tearDown();

    ContextImp *contextImp = nullptr;
    WhiteBox<::L0::CommandList> *commandList = nullptr;
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
};

} // namespace ult
} // namespace L0
