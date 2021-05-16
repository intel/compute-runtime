/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

using ::testing::Return;

namespace L0 {
namespace ult {

Mock<Module>::Mock(::L0::Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type) : WhiteBox(device, moduleBuildLog, type) { EXPECT_CALL(*this, getMaxGroupSize).WillRepeatedly(Return(256u)); }

} // namespace ult
} // namespace L0
