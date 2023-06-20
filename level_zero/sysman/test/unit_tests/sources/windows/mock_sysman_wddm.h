/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_wddm.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanMockWddm : public NEO::WddmMock {
  public:
    SysmanMockWddm(NEO::RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}
};

} // namespace ult
} // namespace Sysman
} // namespace L0
