/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_driver.h"

namespace L0 {
namespace ult {

using MockDriver = Mock<L0::ult::Driver>;
using ::testing::Invoke;

Mock<Driver>::Mock() {
    previousDriver = driver;
    driver = this;
    EXPECT_CALL(*this, initialize)
        .WillRepeatedly(Invoke(this, &MockDriver::mockInitialize));
}

Mock<Driver>::~Mock() {
    if (driver == this) {
        driver = previousDriver;
    }
}

} // namespace ult
} // namespace L0
