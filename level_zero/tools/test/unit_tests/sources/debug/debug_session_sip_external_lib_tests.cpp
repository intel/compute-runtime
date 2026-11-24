/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_sip_external_lib.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include <algorithm>
#include <initializer_list>
#include <vector>

namespace L0 {
namespace ult {

using DebugSessionSipExternalLibTest = Test<DebugApiFixture>;

static NEO::OsLibrary *failingSipExternalLibLoad(const NEO::OsLibraryCreateProperties &properties) {
    ADD_FAILURE();
    return nullptr;
}

TEST_F(DebugSessionSipExternalLibTest, GivenSipExternalLibWhenOpenSipWrapperIsCalledThenSessionIsOpenedWithSipMemoryCallbacksAndHandleIsStored) {
    injectSipLib(*neoDevice);
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, device);

    EXPECT_TRUE(session->openSipWrapper(0x1111, 0x2222));

    EXPECT_EQ(sipExternalLib->openSipSessionHandle, session->getSipHandle(0x1111));
    EXPECT_EQ(nullptr, session->getSipHandle(0x3333));
    EXPECT_EQ(&DebugSessionImp::readSipMemory, sipExternalLib->openSipSessionCapturedReadMemory);
    EXPECT_EQ(&DebugSessionImp::writeSipMemory, sipExternalLib->openSipSessionCapturedWriteMemory);
    auto args = static_cast<DebugSessionImp::SipMemoryAccessArgs *>(sipExternalLib->openSipSessionCapturedUserArg);
    ASSERT_NE(nullptr, args);
    EXPECT_EQ(session.get(), args->debugSession);
    EXPECT_EQ(0x1111u, args->contextHandle);
    EXPECT_EQ(0x2222u, args->gpuVa);
}

TEST_F(DebugSessionSipExternalLibTest, GivenOpenedSipSessionWhenOpenSipWrapperIsCalledForSameContextThenPreviousSessionIsClosedAndReplaced) {
    injectSipLib(*neoDevice);
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, device);
    sipExternalLib->openSipSessionHandle = reinterpret_cast<void *>(0xaaaa);
    EXPECT_TRUE(session->openSipWrapper(0x1111, 0x2222));
    sipExternalLib->openSipSessionHandle = reinterpret_cast<void *>(0xbbbb);

    EXPECT_TRUE(session->openSipWrapper(0x1111, 0x3333));

    EXPECT_EQ((std::vector<void *>{reinterpret_cast<void *>(0xaaaa)}), sipExternalLib->closedSipHandles);
    EXPECT_EQ(reinterpret_cast<void *>(0xbbbb), session->getSipHandle(0x1111));
    EXPECT_EQ(0x3333u, static_cast<DebugSessionImp::SipMemoryAccessArgs *>(sipExternalLib->openSipSessionCapturedUserArg)->gpuVa);
}

TEST_F(DebugSessionSipExternalLibTest, GivenOpenSipSessionFailsWhenOpenSipWrapperIsCalledThenFalseIsReturnedAndNothingIsStored) {
    injectSipLib(*neoDevice);
    sipExternalLib->openSipSessionRetValue = false;
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, device);

    EXPECT_FALSE(session->openSipWrapper(0x1111, 0x2222));

    EXPECT_TRUE(session->sipSessions.empty());
}

TEST_F(DebugSessionSipExternalLibTest, GivenOpenedSipSessionsWhenCloseExternalSipHandlesIsCalledThenEverySessionIsClosedAndCleared) {
    injectSipLib(*neoDevice);
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, device);
    sipExternalLib->openSipSessionHandle = reinterpret_cast<void *>(0xaaaa);
    EXPECT_TRUE(session->openSipWrapper(0x1111, 0x2222));
    sipExternalLib->openSipSessionHandle = reinterpret_cast<void *>(0xbbbb);
    EXPECT_TRUE(session->openSipWrapper(0x3333, 0x4444));

    session->closeExternalSipHandles();

    std::sort(sipExternalLib->closedSipHandles.begin(), sipExternalLib->closedSipHandles.end());
    EXPECT_EQ((std::vector<void *>{reinterpret_cast<void *>(0xaaaa), reinterpret_cast<void *>(0xbbbb)}), sipExternalLib->closedSipHandles);
    EXPECT_TRUE(session->sipSessions.empty());
}

TEST_F(DebugSessionSipExternalLibTest, GivenOpenedSipSessionWhenDebugSessionIsDestroyedThenSipSessionIsClosed) {
    injectSipLib(*neoDevice);
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, device);
    EXPECT_TRUE(session->openSipWrapper(0x1111, 0x2222));

    session.reset();

    EXPECT_EQ((std::vector<void *>{sipExternalLib->openSipSessionHandle}), sipExternalLib->closedSipHandles);
}

TEST_F(DebugSessionSipExternalLibTest, GivenNoSipSessionsWhenDebugSessionIsDestroyedThenSipExternalLibIsNotLoaded) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup{&NEO::OsLibrary::loadFunc, failingSipExternalLibLoad};
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, device);

    session.reset();
}

struct DebugSessionRegisterAccessPropertiesFixture : public DebugApiFixture {
    void setUp() {
        DebugApiFixture::setUp();
        this->injectSipLib(*this->neoDevice);
        this->session = std::make_unique<MockDebugSessionRegisterAccessProperties>(zet_debug_config_t{}, this->device);
        this->session->allThreads[this->threadId]->stopThread(threadMemoryHandle);
        this->session->sipSessions[threadMemoryHandle].handle = threadSipHandle;
    }

    void tearDown() {
        this->session.reset();
        DebugApiFixture::tearDown();
    }

    std::vector<zet_debug_regset_properties_t> createRegsetProperties(std::initializer_list<zet_debug_regset_type_intel_gpu_t> types) {
        std::vector<zet_debug_regset_properties_t> properties;
        for (auto type : types) {
            zet_debug_regset_properties_t regsetProperties = {};
            regsetProperties.type = type;
            regsetProperties.count = deviceLevelCount;
            properties.push_back(regsetProperties);
        }
        return properties;
    }

    static constexpr uint32_t deviceLevelCount = 7u;
    static constexpr uint64_t threadMemoryHandle = 0x1000u;
    void *const threadSipHandle = reinterpret_cast<void *>(0xabcd);
    std::unique_ptr<MockDebugSessionRegisterAccessProperties> session;
    EuThread::ThreadId threadId{0, 0, 0, 0, 0};
};

using DebugSessionRegisterAccessPropertiesTest = Test<DebugSessionRegisterAccessPropertiesFixture>;

TEST_F(DebugSessionRegisterAccessPropertiesTest, GivenRegsetsWhenGetRegisterAccessPropertiesIsCalledThenOnlyThreadSpecificRegsetsAreQueriedAndUpdated) {
    sipExternalLib->getSipLibRegisterAccessCountPerCall = {16, 32};
    auto props = createRegsetProperties({
        ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_STATUS_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_RANDOM_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_DIRECT_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU,
        ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU,
    });
    auto count = static_cast<uint32_t>(props.size());

    EXPECT_TRUE(session->getRegisterAccessProperties(&threadId, &count, props.data()));

    EXPECT_EQ((std::vector<NEO::SipRegisterType>{NEO::SipRegisterType::eGRF, NEO::SipRegisterType::eScalar}), sipExternalLib->getSipLibRegisterAccessQueriedTypes);
    EXPECT_EQ((std::vector<void *>{threadSipHandle, threadSipHandle}), sipExternalLib->getSipLibRegisterAccessQueriedHandles);
    EXPECT_EQ(16u, props.front().count);
    EXPECT_EQ(32u, props.back().count);
    for (size_t i = 1; i < props.size() - 1; i++) {
        EXPECT_EQ(deviceLevelCount, props[i].count) << "regset type " << props[i].type;
    }
}

TEST_F(DebugSessionRegisterAccessPropertiesTest, GivenFirstRegisterAccessFailsWhenGetRegisterAccessPropertiesIsCalledThenFalseIsReturnedAndNoCountIsUpdated) {
    sipExternalLib->getSipLibRegisterAccessFailingCall = 0u;
    sipExternalLib->getSipLibRegisterAccessCountPerCall = {64, 128};
    auto props = createRegsetProperties({ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU});
    auto count = static_cast<uint32_t>(props.size());

    EXPECT_FALSE(session->getRegisterAccessProperties(&threadId, &count, props.data()));

    EXPECT_EQ(1u, sipExternalLib->getSipLibRegisterAccessQueriedTypes.size());
    EXPECT_EQ(deviceLevelCount, props[0].count);
    EXPECT_EQ(deviceLevelCount, props[1].count);
}

TEST_F(DebugSessionRegisterAccessPropertiesTest, GivenSecondRegisterAccessFailsWhenGetRegisterAccessPropertiesIsCalledThenFalseIsReturnedAndOnlyFirstCountIsUpdated) {
    sipExternalLib->getSipLibRegisterAccessFailingCall = 1u;
    sipExternalLib->getSipLibRegisterAccessCountPerCall = {8, 64};
    auto props = createRegsetProperties({ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU});
    auto count = static_cast<uint32_t>(props.size());

    EXPECT_FALSE(session->getRegisterAccessProperties(&threadId, &count, props.data()));

    EXPECT_EQ(2u, sipExternalLib->getSipLibRegisterAccessQueriedTypes.size());
    EXPECT_EQ(8u, props[0].count);
    EXPECT_EQ(deviceLevelCount, props[1].count);
}

TEST_F(DebugSessionSipExternalLibTest, GivenThreadWhenGetThreadSipCounterIsCalledThenCounterIsReadWithThreadMemoryHandle) {
    auto session = std::make_unique<MockDebugSession>(zet_debug_config_t{}, device);
    session->skipGetThreadSipCounter = true;
    session->mockSipCounter = 0x55;
    auto &thread = *session->allThreads.begin()->second;
    constexpr uint64_t memoryHandle = 0x1234;
    thread.stopThread(memoryHandle);
    const char stateSaveArea[8] = {};
    const NEO::StateSaveAreaHeader stateSaveAreaHeader = {};
    uint64_t sipThreadCounter = 0;

    EXPECT_TRUE(session->getThreadSipCounter(stateSaveArea, &thread, &stateSaveAreaHeader, &sipThreadCounter));

    EXPECT_EQ(1u, session->getThreadSipCounterWithMemHandleCalled);
    EXPECT_EQ(memoryHandle, session->getThreadSipCounterWithMemHandleMemoryHandle);
    EXPECT_EQ(&thread, session->getThreadSipCounterWithMemHandleThread);
    EXPECT_EQ(stateSaveArea, session->getThreadSipCounterWithMemHandleStateSaveArea);
    EXPECT_EQ(&stateSaveAreaHeader, session->getThreadSipCounterWithMemHandleHeader);
    EXPECT_EQ(0x55u, sipThreadCounter);
}

} // namespace ult
} // namespace L0
