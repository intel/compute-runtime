/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "unit_tests/fixtures/hello_world_fixture.h"

#include "gtest/gtest.h"

#include <cinttypes>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace iOpenCL {
struct SPatchExecutionEnvironment;
}

namespace NEO {
class DispatchInfo;
class MockCommandQueue;
class MockContext;
class MockDevice;
class MockKernel;
class MockProgram;
struct KernelInfo;
struct WorkaroundTable;

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
    std::unique_ptr<NEO::MockDevice> device;
    std::unique_ptr<NEO::MockContext> context;
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    std::unique_ptr<iOpenCL::SPatchExecutionEnvironment> executionEnvironment;
    std::unique_ptr<NEO::MockProgram> program;
    std::unique_ptr<NEO::KernelInfo> kernelInfo;
};

struct ThreadGroupPreemptionEnqueueKernelTest : NEO::PreemptionEnqueueKernelTest {
    void SetUp() override;
    void TearDown() override;

    NEO::HardwareInfo *globalHwInfo;
    NEO::PreemptionMode originalPreemptionMode;

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

struct MidThreadPreemptionEnqueueKernelTest : NEO::PreemptionEnqueueKernelTest {
    void SetUp() override;
    void TearDown() override;

    NEO::HardwareInfo *globalHwInfo;
    NEO::PreemptionMode originalPreemptionMode;

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

struct PreemptionTestHwDetails {
    struct PreemptionModeHashT {
        auto operator()(const NEO::PreemptionMode &preemptionMode) const -> std::underlying_type<NEO::PreemptionMode>::type {
            return static_cast<std::underlying_type<NEO::PreemptionMode>::type>(preemptionMode);
        }
    };

    bool supportsPreemptionProgramming() const {
        return modeToRegValueMap.size() > 0;
    }

    uint32_t regAddress = 0;
    std::unordered_map<NEO::PreemptionMode, uint32_t, PreemptionModeHashT> modeToRegValueMap;
    uint32_t defaultRegValue = 0;
};

template <typename FamilyType>
PreemptionTestHwDetails GetPreemptionTestHwDetails();
