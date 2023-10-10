/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"

#include "level_zero/sysman/source/api/events/sysman_events_imp.h"
#include "level_zero/sysman/source/api/events/windows/sysman_os_events_imp.h"
#include "level_zero/sysman/source/api/global_operations/sysman_global_operations_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockWddmEventsImp : public WddmEventsImp {
  public:
    bool mockSignalledEventList[KmdSysman::Events::MaxEvents];

    MockWddmEventsImp(OsSysman *pOsSysman) : WddmEventsImp(pOsSysman) {
        for (int i = 0; i < KmdSysman::Events::MaxEvents; i++) {
            mockSignalledEventList[i] = false;
        }
    }

    void mockSignalEvent(zes_event_type_flags_t signalledFlag) {

        if (signalledFlag & ZES_EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED) {
            mockSignalledEventList[KmdSysman::Events::EnergyThresholdCrossed] = true;
        }

        if (signalledFlag & ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER) {
            mockSignalledEventList[KmdSysman::Events::EnterD3] = true;
        }

        if (signalledFlag & ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_EXIT) {
            mockSignalledEventList[KmdSysman::Events::EnterD0] = true;
        }

        if (signalledFlag & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
            mockSignalledEventList[KmdSysman::Events::EnterTDR] = true;
        }

        if (signalledFlag & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
            mockSignalledEventList[KmdSysman::Events::ExitTDR] = true;
        }
    }

    HANDLE createWddmEvent(
        LPSECURITY_ATTRIBUTES lpEventAttributes,
        BOOL bManualReset,
        BOOL bInitialState,
        LPCWSTR lpName) override {
        return NEO::SysCalls::createEvent(lpEventAttributes, bManualReset, bInitialState, (LPCSTR)lpName);
    }

    void resetWddmEvent(HANDLE resetHandle) override {
    }

    void setWddmEvent(HANDLE setHandle) override {
    }

    void closeWddmHandle(HANDLE exitHandle) override {
        NEO::SysCalls::closeHandle(exitHandle);
    }

    uint32_t waitForMultipleWddmEvents(_In_ DWORD nCount,
                                       _In_reads_(nCount) CONST HANDLE *lpHandles,
                                       _In_ BOOL bWaitAll,
                                       _In_ DWORD dwMilliseconds) override {

        for (uint32_t k = 0; k < nCount; k++) {
            for (uint32_t j = 0; j < eventList.size(); j++) {

                if (eventList[j].windowsHandle == lpHandles[k]) {

                    if (eventList[j].id & ZES_EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED) {
                        if (mockSignalledEventList[KmdSysman::Events::EnergyThresholdCrossed] == true) {
                            return j;
                        }
                    }

                    if (eventList[j].id & ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER) {
                        if (mockSignalledEventList[KmdSysman::Events::EnterD3] == true) {
                            return j;
                        }
                    }

                    if (eventList[j].id & ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_EXIT) {
                        if (mockSignalledEventList[KmdSysman::Events::EnterD0] == true) {
                            return j;
                        }
                    }

                    if (eventList[j].id & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
                        if (mockSignalledEventList[KmdSysman::Events::EnterTDR] == true) {
                            return j;
                        }
                    }

                    if (eventList[j].id & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
                        if (mockSignalledEventList[KmdSysman::Events::ExitTDR] == true) {
                            return j;
                        }
                    }
                }
            }
        }
        return WAIT_TIMEOUT;
    }
};

class SysmanEventsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockKmdSysManager> pKmdSysManager;
    std::unique_ptr<GlobalOperationsImp> pGlobalOperations;
    OsEvents *pOsEventsPrev = nullptr;
    L0::Sysman::EventsImp *pEventsImp;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    GlobalOperations *pGlobalOperationsOld = nullptr;
    MockWddmEventsImp *pWddmEventsImp;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new MockKmdSysManager);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pEventsImp = static_cast<L0::Sysman::EventsImp *>(pSysmanDeviceImp->pEvents);
        pOsEventsPrev = pEventsImp->pOsEvents;

        pWddmEventsImp = new MockWddmEventsImp(pOsSysman);
        pEventsImp->pOsEvents = pWddmEventsImp;
        pGlobalOperations = std::make_unique<GlobalOperationsImp>(pWddmSysmanImp);
        pGlobalOperationsOld = pSysmanDeviceImp->pGlobalOperations;
        pSysmanDeviceImp->pGlobalOperations = pGlobalOperations.get();
        pSysmanDeviceImp->pGlobalOperations->init();
        pEventsImp->init();
    }

    void TearDown() override {
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
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(pSysmanDevice->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    std::unique_ptr<zes_device_handle_t[]> phDevices(new zes_device_handle_t[1]);

    phDevices[0] = pSysmanDevice->toHandle();
    uint32_t numDeviceEvents = 0;
    std::unique_ptr<zes_event_type_flags_t[]> pDeviceEvents(new zes_event_type_flags_t[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 100u, 1u, phDevices.get(), &numDeviceEvents, pDeviceEvents.get()));
    EXPECT_EQ(1u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventOnInfiniteWait) {
    init(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(pSysmanDevice->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    std::unique_ptr<zes_device_handle_t[]> phDevices(new zes_device_handle_t[1]);

    phDevices[0] = pSysmanDevice->toHandle();
    uint32_t numDeviceEvents = 0;
    std::unique_ptr<zes_event_type_flags_t[]> pDeviceEvents(new zes_event_type_flags_t[1]);

    pWddmEventsImp->mockSignalEvent(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), INFINITE, 1u, phDevices.get(), &numDeviceEvents, pDeviceEvents.get()));
    EXPECT_EQ(1u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventOnInfiniteWaitMultipleTimes) {
    init(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(pSysmanDevice->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    std::unique_ptr<zes_device_handle_t[]> phDevices(new zes_device_handle_t[1]);
    phDevices[0] = pSysmanDevice->toHandle();
    uint32_t numDeviceEvents = 0;
    std::unique_ptr<zes_event_type_flags_t[]> pDeviceEvents(new zes_event_type_flags_t[1]);

    pWddmEventsImp->mockSignalEvent(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), INFINITE, 1u, phDevices.get(), &numDeviceEvents, pDeviceEvents.get()));
    EXPECT_EQ(1u, numDeviceEvents);

    pWddmEventsImp->mockSignalEvent(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), INFINITE, 1u, phDevices.get(), &numDeviceEvents, pDeviceEvents.get()));
    EXPECT_EQ(1u, numDeviceEvents);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
