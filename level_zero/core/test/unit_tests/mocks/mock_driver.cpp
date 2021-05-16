/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"

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
