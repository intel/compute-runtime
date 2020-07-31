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

Mock<Driver>::Mock() {
    previousDriver = driver;
    driver = this;
}

Mock<Driver>::~Mock() {
    driver = previousDriver;
}

} // namespace ult
} // namespace L0
