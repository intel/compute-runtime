/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_module.h"

using ::testing::Return;

namespace L0 {
namespace ult {

Mock<Module>::Mock(::L0::Device *device, ModuleBuildLog *moduleBuildLog) : WhiteBox(device, moduleBuildLog) { EXPECT_CALL(*this, getMaxGroupSize).WillRepeatedly(Return(256u)); }

} // namespace ult
} // namespace L0
