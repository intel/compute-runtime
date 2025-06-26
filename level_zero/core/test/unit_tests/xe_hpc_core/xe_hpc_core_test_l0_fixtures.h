/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

namespace NEO {
struct HardwareInfo;
} // namespace NEO

namespace L0 {
namespace ult {
struct DeviceFixtureXeHpcTests : public DeviceFixture {
    void checkIfCallingGetMemoryPropertiesWithNonNullPtrThenMaxClockRateReturnZero(HardwareInfo *hwInfo);
    DebugManagerStateRestore restorer;
};

struct CommandListStatePrefetchXeHpcCore : public ModuleFixture {};
} // namespace ult
} // namespace L0
