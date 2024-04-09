/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/options.h"

namespace NEO {
struct RootDeviceEnvironment;
struct MockAubCenterFixture {

    MockAubCenterFixture() = default;
    MockAubCenterFixture(CommandStreamReceiverType commandStreamReceiverType);

    void setUp() {
    }
    void tearDown() {
    }

    static void setMockAubCenter(RootDeviceEnvironment &rootDeviceEnvironment);
    static void setMockAubCenter(RootDeviceEnvironment &rootDeviceEnvironment, CommandStreamReceiverType commandStreamReceiverType);

  protected:
    CommandStreamReceiverType commandStreamReceiverType = CommandStreamReceiverType::aub;
};
} // namespace NEO
