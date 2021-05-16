/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

#include "shared/source/device/device.h"

#include "level_zero/tools/source/debug/debug_session.h"

#include "gmock/gmock.h"

namespace L0 {
namespace ult {

using ::testing::AnyNumber;
using ::testing::Return;

Mock<Device>::Mock() {
    ON_CALL(*this, getMOCS(false, false)).WillByDefault(Return(0));
    EXPECT_CALL(*this, getMOCS(false, false)).Times(AnyNumber());
    ON_CALL(*this, getMOCS(true, false)).WillByDefault(Return(2));
    EXPECT_CALL(*this, getMOCS(true, false)).Times(AnyNumber());

    EXPECT_CALL(*this, getMaxNumHwThreads).WillRepeatedly(Return(16));
}

Mock<Device>::~Mock() {
}

} // namespace ult
} // namespace L0
