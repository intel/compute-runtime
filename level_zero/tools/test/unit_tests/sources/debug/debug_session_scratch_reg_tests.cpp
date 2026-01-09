/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_registers_access.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include "level_zero/zet_intel_gpu_debug.h"

#include "implicit_args.h"

namespace L0 {
namespace ult {

using DebugApiTest = Test<DebugApiFixture>;
TEST_F(DebugApiTest, givenReadThreadScratchRegisterCalledThenSuccessReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);
    setIsScratchInGrf(neoDevice, true);

    L0::Device *l0Device = static_cast<Device *>(device);
    auto session = new MockDebugSession(config, device, true);
    session->initialize();
    l0Device->setDebugSession(session);
    SIP::version version = {3, 0, 0};
    initStateSaveArea(session->stateSaveAreaHeader, version, device);
    ze_device_thread_t stoppedThread = {0, 0, 0, 0};
    EuThread::ThreadId stoppedThreadId{0, stoppedThread};
    session->allThreads[stoppedThreadId]->stopThread(1u);
    session->allThreads[stoppedThreadId]->reportAsStopped();
    session->readThreadScratchRegistersResult = ZE_RESULT_SUCCESS;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugReadRegisters(session->toHandle(), {0, 0, 0, 0}, ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU, 0, 1, nullptr));
}

using DebugSessionRegistersAccessTestScratchV3 = Test<DebugSessionRegistersAccessScratchV3>;

TEST_F(DebugSessionRegistersAccessTestScratchV3, WhenReadingThreadScratchRegThenErrorsAreHandled) {
    {
        auto pStateSaveAreaHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeaderV3.state_area_offset +
                    pStateSaveAreaHeader->regHeaderV3.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto threadScratchRegDesc = DebugSessionImp::getThreadScratchRegsetDesc();
    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    uint64_t scratch[2];
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, session->readThreadScratchRegisters(threadId, 0, threadScratchRegDesc->num + 1, scratch));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, session->readThreadScratchRegisters(threadId, threadScratchRegDesc->num, 1, scratch));

    session->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readThreadScratchRegisters(threadId, 0, 1, scratch));
    session->readRegistersResult = ZE_RESULT_FORCE_UINT32;

    dumpRegisterState();

    auto threadSlotOffset = session->calculateThreadSlotOffset(threadId);
    auto stateSaveAreaHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    auto grfOffset = threadSlotOffset + stateSaveAreaHeader->regHeaderV3.grf.offset;
    uint32_t *r0Thread0 = reinterpret_cast<uint32_t *>(ptrOffset(session->stateSaveAreaHeader.data(), grfOffset));

    r0Thread0[15] = 0u;
    r0Thread0[14] = 0u;
    // implictArgsGpuVa == 0
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readThreadScratchRegisters(threadId, 0, 2, scratch));
    EXPECT_EQ(0u, scratch[0]);
    EXPECT_EQ(0u, scratch[1]);

    r0Thread0[15] = 0x1111;
    r0Thread0[14] = 0x1111;

    // read implicit args from gpu mem fails
    session->readGpuMemoryCallCount = 0;
    session->forcereadGpuMemoryFailOnCount = 2;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readThreadScratchRegisters(threadId, 0, 1, scratch));

    // scratchPtr == 0
    session->readGpuMemoryCallCount = 0;
    session->forcereadGpuMemoryFailOnCount = 0;

    session->readMemoryBuffer.assign(sizeof(NEO::ImplicitArgs), 5);
    NEO::ImplicitArgsV1 args1 = {};
    args1.header.structVersion = 1;
    args1.header.structSize = NEO::ImplicitArgsV1::getSize();
    args1.scratchPtr = 0u;
    memcpy_s(session->readMemoryBuffer.data(), sizeof(args1), &args1, sizeof(args1));
    r0Thread0[15] = static_cast<uint32_t>((reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()) >> 32) & 0xffffffff);
    r0Thread0[14] = static_cast<uint32_t>(reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()) & 0xffffffff);
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readThreadScratchRegisters(threadId, 0, 2, scratch));
    EXPECT_EQ(0u, scratch[0]);
    EXPECT_EQ(0u, scratch[1]);

    // read render surface fails
    args1.scratchPtr = 0x1234u;
    memcpy_s(session->readMemoryBuffer.data(), sizeof(args1), &args1, sizeof(args1));
    session->readGpuMemoryCallCount = 0;
    session->forcereadGpuMemoryFailOnCount = 3;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readThreadScratchRegisters(threadId, 0, 1, scratch));

    // Unsupported structVersion
    NEO::ImplicitArgsV0 args = {};
    args.header.structVersion = 0;
    args.header.structSize = NEO::ImplicitArgsV0::getSize();
    memcpy_s(session->readMemoryBuffer.data(), sizeof(args), &args, sizeof(args));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readThreadScratchRegisters(threadId, 0, 1, scratch));

    NEO::ImplicitArgsV2 args2 = {};
    args2.header.structVersion = 2;
    memcpy_s(session->readMemoryBuffer.data(), sizeof(args2), &args2, sizeof(args2));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readThreadScratchRegisters(threadId, 0, 1, scratch));
}

TEST_F(DebugSessionRegistersAccessTestScratchV3, WhenReadingThreadScratchRegistersThenCorrectAddressesAreReturned) {
    {
        auto pStateSaveAreaHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeaderV3.state_area_offset +
                    pStateSaveAreaHeader->regHeaderV3.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    session->allThreads[threadId]->stopThread(1u);

    dumpRegisterState();

    auto threadSlotOffset = session->calculateThreadSlotOffset(threadId);
    auto stateSaveAreaHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    auto grfOffset = threadSlotOffset + stateSaveAreaHeader->regHeaderV3.grf.offset;
    uint32_t *r0Thread0 = reinterpret_cast<uint32_t *>(ptrOffset(session->stateSaveAreaHeader.data(), grfOffset));

    uint64_t scratch[2];
    uint64_t scratchExpected[2] = {0xdeadbeef, 0xbeefdead};

    constexpr auto implicitArgsAlignedSize = NEO::ImplicitArgsV1::getAlignedSize();
    session->readMemoryBuffer.assign(4 * gfxCoreHelper.getRenderSurfaceStateSize() + implicitArgsAlignedSize, 5);
    NEO::ImplicitArgsV1 args = {};
    args.header.structVersion = 1;
    args.header.structSize = NEO::ImplicitArgsV1::getSize();

    args.scratchPtr = reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()) + implicitArgsAlignedSize;
    memcpy_s(session->readMemoryBuffer.data(), sizeof(args), &args, sizeof(args));

    r0Thread0[15] = static_cast<uint32_t>((reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()) >> 32) & 0xffffffff);
    r0Thread0[14] = static_cast<uint32_t>(reinterpret_cast<uint64_t>(session->readMemoryBuffer.data()) & 0xffffffff);

    const uint32_t ptss = 128;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                          &session->readMemoryBuffer[implicitArgsAlignedSize], 1, scratchExpected[0], 0,
                                                          ptss, nullptr, false, 6, false, true);
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                          &session->readMemoryBuffer[implicitArgsAlignedSize + gfxCoreHelper.getRenderSurfaceStateSize()], 1, scratchExpected[1], 0,
                                                          ptss, nullptr, false, 6, false, true);

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readThreadScratchRegisters(threadId, 0, 2, scratch));
    EXPECT_EQ(scratchExpected[0], scratch[0]);
    EXPECT_EQ(scratchExpected[1], scratch[1]);
    scratch[0] = 0;
    scratch[1] = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readThreadScratchRegisters(threadId, 0, 1, scratch));
    EXPECT_EQ(scratchExpected[0], scratch[0]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readThreadScratchRegisters(threadId, 1, 1, &scratch[1]));
    EXPECT_EQ(scratchExpected[1], scratch[1]);
}

struct DebugSessionScratchRegistersTestV2 : public ::testing::Test {
    struct MockDebugSessionScratch : public MockDebugSession {
        using MockDebugSession::getScratchRenderSurfaceStateAddress;
        using MockDebugSession::MockDebugSession;

        uint32_t getRegisterSize(uint32_t type) override {
            EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, type);
            return sizeof(uint32_t);
        }

        struct ReadRegistersParams {
            EuThread::ThreadId thread;
            uint32_t type, start, count;
            void *pRegisters;
        };
        std::optional<ReadRegistersParams> lastReadRegistersParams;
        std::vector<uint8_t> readRegistersData;
        ze_result_t readRegistersResult = ZE_RESULT_SUCCESS;
        ze_result_t readRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisters) override {
            lastReadRegistersParams = {.thread = thread, .type = type, .start = start, .count = count, .pRegisters = pRegisters};
            if (readRegistersResult == ZE_RESULT_SUCCESS) {
                size_t sizeToCopy = std::min(readRegistersData.size(), static_cast<size_t>(count * getRegisterSize(type)));
                memcpy(pRegisters, readRegistersData.data(), sizeToCopy);
            }
            return readRegistersResult;
        }

        std::optional<EuThread::ThreadId> lastGetRssAddressThread;
        std::optional<uint64_t> getScratchRenderSurfaceStateAddressV2Result;
        std::optional<ze_result_t> getScratchRenderSurfaceStateAddressV2Return;
        ze_result_t getScratchRenderSurfaceStateAddressV2(EuThread::ThreadId thread, uint64_t *result) override {
            lastGetRssAddressThread = thread;
            if (getScratchRenderSurfaceStateAddressV2Return.has_value()) {
                if (getScratchRenderSurfaceStateAddressV2Result.has_value()) {
                    *result = getScratchRenderSurfaceStateAddressV2Result.value();
                }
                return getScratchRenderSurfaceStateAddressV2Return.value();
            }
            return MockDebugSession::getScratchRenderSurfaceStateAddressV2(thread, result);
        }
    };
    static constexpr uint64_t testMemoryHandle = 0xABCDEF00ULL;
    DebugSessionScratchRegistersTestV2() : neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0)),
                                           mockDevice(std::make_unique<MockDeviceImp>(neoDevice)),
                                           session(zet_debug_config_t{}, static_cast<L0::Device *>(mockDevice.get())),
                                           threadId(5, 7, 9, 3, 1) {
        session.allThreads[threadId] = std::make_unique<EuThread>(threadId);
        session.allThreads[threadId]->stopThread(testMemoryHandle);
        setIsScratchInGrf(neoDevice, false);
    }

    NEO::MockDevice *neoDevice; // mockDevice takes ownership of neoDevice
    std::unique_ptr<MockDeviceImp> mockDevice;
    MockDebugSessionScratch session;
    EuThread::ThreadId threadId;
};

TEST_F(DebugSessionScratchRegistersTestV2, WhenCallingGetRenderSurfaceStateAddressThenV2IsCalled) {
    uint64_t rssAddress = 0x123456780ULL;
    session.getScratchRenderSurfaceStateAddressV2Result = rssAddress;
    session.getScratchRenderSurfaceStateAddressV2Return = ZE_RESULT_SUCCESS;

    uint64_t result = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session.getScratchRenderSurfaceStateAddress(threadId, &result));
    EXPECT_EQ(rssAddress, result);
    EXPECT_EQ(threadId, session.lastGetRssAddressThread.value());
}

TEST_F(DebugSessionScratchRegistersTestV2, WhenGetRenderSurfaceStateAddressV2ReturnsErrorThenErrorIsPropagated) {
    session.getScratchRenderSurfaceStateAddressV2Return = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    uint64_t result = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session.getScratchRenderSurfaceStateAddress(threadId, &result));
    EXPECT_EQ(threadId, session.lastGetRssAddressThread.value());
}

TEST_F(DebugSessionScratchRegistersTestV2, WhenReadRegistersImpReturnsErrorThenGetRenderSurfaceStateAddressV2ReturnsError) {
    session.readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;

    uint64_t result = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session.getScratchRenderSurfaceStateAddress(threadId, &result));
    EXPECT_EQ(threadId, session.lastReadRegistersParams->thread);
}

TEST_F(DebugSessionScratchRegistersTestV2, WhenGetRenderSurfaceStateAddressV2IsCalledThenCorrectResultIsReturned) {
    session.readRegistersData.assign({0x0a, 0xbc, 0xde, 0xf0, 0x0d, 0xbe, 0xad, 0xde});

    uint64_t result = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session.getScratchRenderSurfaceStateAddress(threadId, &result));
    EXPECT_EQ(0xf0debc0adeadbe0d, result);
    EXPECT_EQ(threadId, session.lastReadRegistersParams->thread);
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, session.lastReadRegistersParams->type);
    EXPECT_EQ(18u, session.lastReadRegistersParams->start);
    EXPECT_EQ(2u, session.lastReadRegistersParams->count);
}

} // namespace ult
} // namespace L0
