/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/events/events_imp.h"
#include "level_zero/tools/source/sysman/events/windows/os_events_imp.h"
#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

using ::testing::Matcher;

namespace L0 {
namespace ult {

class SysmanEventsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<MockKmdSysManager>> pKmdSysManager;
    std::unique_ptr<GlobalOperationsImp> pGlobalOperations;
    OsEvents *pOsEventsPrev = nullptr;
    L0::EventsImp *pEventsImp;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    GlobalOperations *pGlobalOperationsOld = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new Mock<MockKmdSysManager>);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        EXPECT_CALL(*pKmdSysManager, escape(_, _, _, _, _))
            .WillRepeatedly(::testing::Invoke(pKmdSysManager.get(), &Mock<MockKmdSysManager>::mock_escape));

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pEventsImp = static_cast<L0::EventsImp *>(pSysmanDeviceImp->pEvents);
        pOsEventsPrev = pEventsImp->pOsEvents;
        pEventsImp->pOsEvents = nullptr;
        pGlobalOperations = std::make_unique<GlobalOperationsImp>(pWddmSysmanImp);
        pGlobalOperationsOld = pSysmanDeviceImp->pGlobalOperations;
        pSysmanDeviceImp->pGlobalOperations = pGlobalOperations.get();
        pSysmanDeviceImp->pGlobalOperations->init();
        pEventsImp->init();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        if (nullptr != pEventsImp->pOsEvents) {
            delete pEventsImp->pOsEvents;
        }
        pEventsImp->pOsEvents = pOsEventsPrev;
        pEventsImp = nullptr;
        pSysmanDeviceImp->pGlobalOperations = pGlobalOperationsOld;
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterTimingOutWithNoEvent) {
    init(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 100u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventOnInfiniteWait) {
    init(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];

    pKmdSysManager->signalEvent(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), INFINITE, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventOnInfiniteWaitMultipleTimes) {
    init(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];

    pKmdSysManager->signalEvent(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), INFINITE, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);

    pKmdSysManager->signalEvent(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), INFINITE, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
}

} // namespace ult
} // namespace L0
