/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_registers_access.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include "level_zero/zet_intel_gpu_debug.h"

namespace L0 {
namespace ult {

struct Xe3AndLaterCoreDebugSessionTest : public L0::ult::DebugSessionRegistersAccessV3 {
    void setUp() override {
        L0::ult::DebugSessionRegistersAccessV3::setUp();
        auto mockBuiltins = new L0::ult::MockBuiltins();
        mockBuiltins->stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(3, 128);
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltins);
        {
            auto pStateSaveAreaHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
            auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                        pStateSaveAreaHeader->regHeaderV3.state_area_offset +
                        pStateSaveAreaHeader->regHeaderV3.state_save_size * 16;
            session->stateSaveAreaHeader.resize(size);
        }
        thread = stoppedThread;
        regdesc = &(reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeaderV3.sr;
    }
    uint32_t sr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    SIP::regset_desc *regdesc = nullptr;
    ze_device_thread_t thread;
};

using Xe3AndLaterCoreDebugSessionTestFixture = Test<Xe3AndLaterCoreDebugSessionTest>;

HWTEST2_F(Xe3AndLaterCoreDebugSessionTestFixture,
          GivenSrRegisterWhenGetThreadRegisterSetPropertiesCalledThenCorrectRegisterCountIsReported, IsXe3Core) {
    std::vector<uint32_t> checkRegCount{32, 64, 96, 128, 160, 192, 256};
    for (size_t i = 0; i < checkRegCount.size(); i++) {
        sr0[4] = 0xFFFFFC00 | checkRegCount[i];
        session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, sr0, true);
        uint32_t threadCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
        std::vector<zet_debug_regset_properties_t> threadRegsetProps(threadCount);
        ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, threadRegsetProps.data()));
        EXPECT_EQ(checkRegCount[i], threadRegsetProps[0].count);
    }
}

HWTEST2_F(Xe3AndLaterCoreDebugSessionTestFixture,
          GivenSrRegisterWhenGetThreadRegisterSetPropertiesCalledThenCorrectRegisterCountIsReported, IsXe3pCore) {
    std::vector<uint32_t> checkRegCount{32, 64, 96, 128, 160, 192, 256, 512};
    for (size_t i = 0; i < checkRegCount.size(); i++) {
        sr0[4] = 0xFFFFFC00 | checkRegCount[i];
        session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, sr0, true);
        uint32_t threadCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
        std::vector<zet_debug_regset_properties_t> threadRegsetProps(threadCount);
        ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, threadRegsetProps.data()));
        EXPECT_EQ(checkRegCount[i], threadRegsetProps[0].count);
    }
}

using DebugSessionRegistersAccessTesXe3AndLaterSpecfic = Test<DebugSessionRegistersAccess>;
HWTEST2_F(DebugSessionRegistersAccessTesXe3AndLaterSpecfic, GivenGetThreadRegisterSetPropertiesCalledExceptGrfCountOtherPropertieAreSameAsGetRegisterSetProperties,
          IsAtLeastXe3Core) {

    auto mockBuiltins = new MockBuiltins();
    mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltins);

    uint32_t count = 0;
    uint32_t threadCount = 0;
    ze_device_thread_t thread = stoppedThread;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetRegisterSetProperties(session->getConnectedDevice(), &count, nullptr));
    EXPECT_EQ(13u, count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
    ASSERT_EQ(threadCount, count);

    std::vector<zet_debug_regset_properties_t> regsetProps(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetRegisterSetProperties(session->getConnectedDevice(), &count, regsetProps.data()));
    std::vector<zet_debug_regset_properties_t> threadRegsetProps(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &count, threadRegsetProps.data()));

    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(regsetProps[i].stype, threadRegsetProps[i].stype);
        EXPECT_EQ(regsetProps[i].pNext, threadRegsetProps[i].pNext);
        EXPECT_EQ(regsetProps[i].version, threadRegsetProps[i].version);
        EXPECT_EQ(regsetProps[i].generalFlags, threadRegsetProps[i].generalFlags);
        EXPECT_EQ(regsetProps[i].deviceFlags, threadRegsetProps[i].deviceFlags);
        if (regsetProps[i].type != ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU) {
            EXPECT_EQ(regsetProps[i].count, threadRegsetProps[i].count);
        }
        EXPECT_EQ(regsetProps[i].bitSize, threadRegsetProps[i].bitSize);
        EXPECT_EQ(regsetProps[i].byteSize, threadRegsetProps[i].byteSize);
    }
}

} // namespace ult
} // namespace L0
