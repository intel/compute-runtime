/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/events/linux/mock_events.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;
namespace L0 {
namespace ult {

class SysmanEventsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockEventsFsAccess> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;
    OsEvents *pOsEventsPrev = nullptr;
    L0::EventsImp *pEventsImp;
    GlobalOperations *pGlobalOperationsOriginal = nullptr;
    std::unique_ptr<GlobalOperationsImp> pGlobalOperations;
    std::unique_ptr<MockEventsSysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOriginal = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEventsFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pEventsImp = static_cast<L0::EventsImp *>(pSysmanDeviceImp->pEvents);
        pOsEventsPrev = pEventsImp->pOsEvents;
        pEventsImp->pOsEvents = nullptr;
        pGlobalOperations = std::make_unique<GlobalOperationsImp>(pLinuxSysmanImp);
        pGlobalOperationsOriginal = pSysmanDeviceImp->pGlobalOperations;
        pSysmanDeviceImp->pGlobalOperations = pGlobalOperations.get();
        pSysmanDeviceImp->pGlobalOperations->init();

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
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
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pSysmanDeviceImp->pGlobalOperations = pGlobalOperationsOriginal;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    pFsAccess->mockReadVal = 1;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    pFsAccess->mockReadVal = 0;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    pFsAccess->mockReadStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForCurrentlyUnsupportedEventsThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_TEMP_THRESHOLD2));
    pFsAccess->mockReadVal = 1;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenReadSymLinkCallFailsWhenGettingPCIBDFThenEmptyPciIdPathTagReceived) {
    pSysfsAccess->mockReadSymLinkFailureError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    PublicLinuxEventsImp linuxEventImp(pOsSysman);
    EXPECT_TRUE(linuxEventImp.pciIdPathTag.empty());
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceDetachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    pFsAccess->mockReadVal = 1;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceDetachEventsThenAfterReceivingEventRegisterEventAgainToReceiveEvent) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    pFsAccess->mockReadVal = 1;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH, pDeviceEvents[0]);
    numDeviceEvents = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceDetachEventsThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    pFsAccess->mockReadVal = 0;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    pFsAccess->mockReadStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    pFsAccess->mockReadVal = 1;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    pFsAccess->mockReadVal = 0;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    pFsAccess->mockReadStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForMemHealthEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    PublicLinuxEventsImp *pLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pLinuxEventsImp->eventRegister(ZES_EVENT_TYPE_FLAG_MEM_HEALTH);
    pLinuxEventsImp->memHealthAtEventRegister = ZES_MEM_HEALTH_OK;
    zes_event_type_flags_t events = 0;
    uint32_t timeout = 1u;
    EXPECT_TRUE(pLinuxEventsImp->eventListen(events, timeout));
    EXPECT_EQ(events, ZES_EVENT_TYPE_FLAG_MEM_HEALTH);
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForMemHealthEventsAndMemHealthDidntOccurThenEventListenAPIReturnsWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForAListOfEventsThenEventRegisterAPIReturnsProperErrorCodeInCaseEventsAreInvalid) {
    zes_event_type_flags_t events1 = 0x7ffe;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), events1));
    zes_event_type_flags_t events2 = 0x1e240;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesDeviceEventRegister(device->toHandle(), events2));
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenExAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    pFsAccess->mockReadVal = 1;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenExAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    pFsAccess->mockReadVal = 0;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    pFsAccess->mockReadStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
}

} // namespace ult
} // namespace L0
