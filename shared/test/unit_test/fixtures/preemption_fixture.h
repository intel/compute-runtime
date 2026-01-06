/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

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

inline constexpr uint64_t preemptionModeMask = makeBitMask<17, 18>();
inline constexpr uint32_t midThreadMode = static_cast<uint32_t>(preemptionModeMask);
inline constexpr uint32_t threadGroupMode = static_cast<uint32_t>(setBits(preemptionModeMask, true, 0b10));
inline constexpr uint32_t midBatchMode = static_cast<uint32_t>(setBits(preemptionModeMask, true, 0b100));

template <typename FamilyType>
PreemptionTestHwDetails getPreemptionTestHwDetails();