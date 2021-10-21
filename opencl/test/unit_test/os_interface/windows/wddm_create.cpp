/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_wddm.h"

namespace NEO {
Wddm *Wddm::createWddm(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    return new WddmMock(rootDeviceEnvironment);
}
} // namespace NEO
