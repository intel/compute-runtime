/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_device.h"

#include "shared/source/device/device.h"

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
