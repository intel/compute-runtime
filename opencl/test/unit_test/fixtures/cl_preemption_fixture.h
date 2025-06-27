/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

#include <memory>

namespace NEO {
class DispatchInfo;
class MockCommandQueue;
class MockContext;
class MockDevice;
class MockKernel;
class MockProgram;
struct KernelInfo;
struct WorkaroundTable;
class MockClDevice;
struct HardwareInfo;

using PreemptionEnqueueKernelFixture = HelloWorldFixture<HelloWorldFixtureFactory>;
using PreemptionEnqueueKernelTest = Test<PreemptionEnqueueKernelFixture>;
} // namespace NEO

class DevicePreemptionTests : public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    DevicePreemptionTests();
    ~DevicePreemptionTests() override;

    NEO::PreemptionMode preemptionMode;
    NEO::WorkaroundTable *waTable = nullptr;
    std::unique_ptr<NEO::DispatchInfo> dispatchInfo;
    std::unique_ptr<NEO::MockKernel> kernel;
    std::unique_ptr<NEO::MockCommandQueue> cmdQ;
    std::unique_ptr<NEO::MockClDevice> device;
    std::unique_ptr<NEO::MockContext> context;
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    std::unique_ptr<NEO::MockProgram> program;
    std::unique_ptr<NEO::KernelInfo> kernelInfo;
    const uint32_t rootDeviceIndex = 0u;
};

struct ThreadGroupPreemptionEnqueueKernelTest : NEO::PreemptionEnqueueKernelTest {
    void SetUp() override;
    void TearDown() override;

    NEO::HardwareInfo *globalHwInfo = nullptr;
    NEO::PreemptionMode originalPreemptionMode = NEO::PreemptionMode::Initial;

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

struct MidThreadPreemptionEnqueueKernelTest : NEO::PreemptionEnqueueKernelTest {
    void SetUp() override;
    void TearDown() override;

    NEO::HardwareInfo *globalHwInfo = nullptr;
    NEO::PreemptionMode originalPreemptionMode = NEO::PreemptionMode::Initial;

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};
