/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
struct ContextImp;
struct Device;

namespace ult {

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
    VariableBackup<bool> backupLocalMemory;
};

} // namespace ult
} // namespace L0
