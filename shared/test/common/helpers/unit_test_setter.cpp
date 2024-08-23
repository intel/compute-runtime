/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

void UnitTestSetter::disableHeapless([[maybe_unused]] const DebugManagerStateRestore &restorer){};

void UnitTestSetter::disableHeaplessStateInit([[maybe_unused]] const DebugManagerStateRestore &restorer){};

void UnitTestSetter::setCcsExposure(RootDeviceEnvironment &rootDeviceEnvironment) {}

void UnitTestSetter::setRcsExposure(RootDeviceEnvironment &rootDeviceEnvironment) {}
} // namespace NEO
