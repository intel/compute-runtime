/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_registers_access.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

using Xe2HpgCoreDebugSessionTest = Test<L0::ult::DebugSessionRegistersAccess>;

HWTEST2_F(Xe2HpgCoreDebugSessionTest,
          givenGetThreadRegisterSetPropertiesCalledWhenLargeGrfIsSetThen256GrfRegisterCountIsReported, IsXe2HpgCore) {
    auto mockBuiltins = new L0::ult::MockBuiltins();
    mockBuiltins->stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(2, 256);
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltins);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    ze_device_thread_t thread = stoppedThread;

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.sr;
    uint32_t sr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    sr0[1] = 0x80006000;
    session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, sr0, true);

    uint32_t threadCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
    std::vector<zet_debug_regset_properties_t> threadRegsetProps(threadCount);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, threadRegsetProps.data()));
    EXPECT_EQ(256u, threadRegsetProps[0].count);
}

HWTEST2_F(Xe2HpgCoreDebugSessionTest,
          givenGetThreadRegisterSetPropertiesCalledWhenLargeGrfIsNotSetThen128GrfRegisterCountIsReported, IsXe2HpgCore) {
    auto mockBuiltins = new L0::ult::MockBuiltins();
    mockBuiltins->stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(2, 256);
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltins);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    ze_device_thread_t thread = stoppedThread;

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.sr;
    uint32_t sr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    sr0[1] = 0x80003000;
    session->registersAccessHelper(session->allThreads[stoppedThreadId].get(), regdesc, 0, 1, sr0, true);

    uint32_t threadCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, nullptr));
    std::vector<zet_debug_regset_properties_t> threadRegsetProps(threadCount);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetDebugGetThreadRegisterSetProperties(session->toHandle(), thread, &threadCount, threadRegsetProps.data()));
    EXPECT_EQ(128u, threadRegsetProps[0].count);
}

using DebugApiTest = Test<DebugApiFixture>;
HWTEST2_F(DebugApiTest, givenDeviceWhenDebugAttachIsAvaialbleThenGetPropertiesReturnsCorrectFlag2, IsXe2HpgCore) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);
}

using DebugSessionRegistersAccessTestProductSpecfic = Test<DebugSessionRegistersAccess>;
HWTEST2_F(DebugSessionRegistersAccessTestProductSpecfic, GivenGetThreadRegisterSetPropertiesCalledPropertieAreTheSameAsGetRegisterSetProperties,
          IsAtMostXe2HpgCore) {

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
        EXPECT_EQ(regsetProps[i].count, threadRegsetProps[i].count);
        EXPECT_EQ(regsetProps[i].bitSize, threadRegsetProps[i].bitSize);
        EXPECT_EQ(regsetProps[i].byteSize, threadRegsetProps[i].byteSize);
    }
}

} // namespace ult
} // namespace L0
