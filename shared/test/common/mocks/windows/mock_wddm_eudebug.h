/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_wddm.h"

#pragma once

namespace NEO {

struct WddmEuDebugInterfaceMock : public WddmMock {
    WddmEuDebugInterfaceMock(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}

    bool isDebugAttachAvailable() override {
        return true;
    }
};
} // namespace NEO