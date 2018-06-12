/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/gen_common/test.h"

#include "gtest/gtest.h"

#include <cinttypes>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace iOpenCL {
struct SPatchExecutionEnvironment;
}

namespace OCLRT {
enum class PreemptionMode : uint32_t;
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
} // namespace OCLRT

class DevicePreemptionTests : public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;
    void forceWhitelistedRegs(bool whitelisted);

    DevicePreemptionTests();
    ~DevicePreemptionTests() override;

    OCLRT::PreemptionMode preemptionMode;
    OCLRT::WorkaroundTable *waTable = nullptr;
    std::unique_ptr<OCLRT::DispatchInfo> dispatchInfo;
    std::unique_ptr<OCLRT::MockKernel> kernel;
    std::unique_ptr<OCLRT::MockCommandQueue> cmdQ;
    std::unique_ptr<OCLRT::MockDevice> device;
    std::unique_ptr<OCLRT::MockContext> context;
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    std::unique_ptr<iOpenCL::SPatchExecutionEnvironment> executionEnvironment;
    std::unique_ptr<OCLRT::MockProgram> program;
    std::unique_ptr<OCLRT::KernelInfo> kernelInfo;
};

struct ThreadGroupPreemptionEnqueueKernelTest : OCLRT::PreemptionEnqueueKernelTest {
    void SetUp() override;
    void TearDown() override;

    OCLRT::HardwareInfo *globalHwInfo;
    OCLRT::PreemptionMode originalPreemptionMode;

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

struct MidThreadPreemptionEnqueueKernelTest : OCLRT::PreemptionEnqueueKernelTest {
    void SetUp() override;
    void TearDown() override;

    OCLRT::HardwareInfo *globalHwInfo;
    OCLRT::PreemptionMode originalPreemptionMode;

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

struct PreemptionTestHwDetails {
    struct PreemptionModeHashT {
        auto operator()(const OCLRT::PreemptionMode &preemptionMode) const -> std::underlying_type<OCLRT::PreemptionMode>::type {
            return static_cast<std::underlying_type<OCLRT::PreemptionMode>::type>(preemptionMode);
        }
    };

    bool supportsPreemptionProgramming() const {
        return modeToRegValueMap.size() > 0;
    }

    uint32_t regAddress = 0;
    std::unordered_map<OCLRT::PreemptionMode, uint32_t, PreemptionModeHashT> modeToRegValueMap;
    uint32_t defaultRegValue = 0;
};

template <typename FamilyType>
PreemptionTestHwDetails GetPreemptionTestHwDetails();
