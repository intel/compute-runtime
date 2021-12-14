/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <cinttypes>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace iOpenCL {
struct SPatchExecutionEnvironment;
}

namespace NEO {
class MockCommandQueue;
class MockContext;
class MockDevice;
class MockProgram;
struct KernelInfo;
struct WorkaroundTable;
} // namespace NEO

class DevicePreemptionTests : public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    DevicePreemptionTests();
    ~DevicePreemptionTests() override;

    NEO::PreemptionMode preemptionMode;
    NEO::WorkaroundTable *waTable = nullptr;
    std::unique_ptr<NEO::MockDevice> device;
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    std::unique_ptr<iOpenCL::SPatchExecutionEnvironment> executionEnvironment;
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
