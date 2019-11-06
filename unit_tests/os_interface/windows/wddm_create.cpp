/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_wddm.h"

namespace NEO {
Wddm *Wddm::createWddm(RootDeviceEnvironment &rootDeviceEnvironment) {
    return new WddmMock(rootDeviceEnvironment);
}
} // namespace NEO
