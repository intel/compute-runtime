/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
class MockClDevice;
class MockClExecutionEnvironment;
class MockDevice;
class OsContext;
struct RootDeviceEnvironment;

struct ClDeviceFixture {
    void setUp();
    void setUpImpl(const NEO::HardwareInfo *hardwareInfo);
    void tearDown();

    MockDevice *createWithUsDeviceId(unsigned short usDeviceId);

    template <typename HelperType>
    HelperType &getHelper() const;

    MockDevice *pDevice = nullptr;
    MockClDevice *pClDevice = nullptr;
    volatile TagAddressType *pTagMemory = nullptr;
    HardwareInfo hardwareInfo = {};
    PLATFORM platformHelper = {};
    OsContext *osContext = nullptr;
    const uint32_t rootDeviceIndex = 0u;
    MockClExecutionEnvironment *pClExecutionEnvironment = nullptr;

    const RootDeviceEnvironment &getRootDeviceEnvironment() const;
    RootDeviceEnvironment &getMutableRootDeviceEnvironment();
};

} // namespace NEO
