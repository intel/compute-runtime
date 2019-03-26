/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/windows/gdi_interface.h"

namespace NEO {

class MockGdi : public Gdi {
  public:
    MockGdi() {
        initialized = getAllProcAddresses();
    }
    ~MockGdi(){};

    bool nonZeroNumBytesToTrim = false;

    void setNonZeroNumBytesToTrimInEvict() {
        nonZeroNumBytesToTrim = true;
        initialized = getAllProcAddresses();
    }

    static NTSTATUS __stdcall makeResidentMock(IN OUT D3DDDI_MAKERESIDENT *arg) {
        getMakeResidentArg() = *arg;
        return 0;
    }

    static NTSTATUS __stdcall evictMock(IN D3DKMT_EVICT *arg) {
        getEvictArg() = *arg;
        return 0;
    }

    static NTSTATUS __stdcall evictMockWithNonZeroTrim(IN D3DKMT_EVICT *arg) {
        getEvictArg() = *arg;
        arg->NumBytesToTrim = 4096;

        return 0;
    }

    static NTSTATUS __stdcall registerTrimNotificationMock(IN D3DKMT_REGISTERTRIMNOTIFICATION *arg) {
        getRegisterTrimNotificationArg() = *arg;
        arg->Handle = reinterpret_cast<VOID *>(1);
        return 0;
    }

    static NTSTATUS __stdcall unregisterTrimNotificationMock(IN D3DKMT_UNREGISTERTRIMNOTIFICATION *arg) {
        getUnregisterTrimNotificationArg() = *arg;
        arg->Handle = reinterpret_cast<VOID *>(1);
        return 0;
    }

    static NTSTATUS __stdcall destroyAllocation2Mock(IN D3DKMT_DESTROYALLOCATION2 *arg) {
        getDestroyArg() = *arg;
        return 0;
    }

    static NTSTATUS __stdcall waitFromCpuMock(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *arg) {
        getWaitFromCpuArg() = *arg;
        return 0;
    }

    static NTSTATUS __stdcall createSynchronizationObject2Mock(IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *arg) {
        getCreateSynchronizationObject2Arg() = *arg;
        return 0;
    }

    static NTSTATUS __stdcall queryResourceInfoMock(IN OUT D3DKMT_QUERYRESOURCEINFO *arg) {
        getQueryResourceInfoArgIn() = *arg;
        arg->NumAllocations = getQueryResourceInfoArgOut().NumAllocations;
        arg->ResourcePrivateDriverDataSize = getQueryResourceInfoArgOut().ResourcePrivateDriverDataSize;
        arg->TotalPrivateDriverDataSize = getQueryResourceInfoArgOut().TotalPrivateDriverDataSize;
        return 0;
    }

    static NTSTATUS __stdcall openResourceMock(IN OUT D3DKMT_OPENRESOURCE *arg) {
        getOpenResourceArgIn() = *arg;
        if (arg->NumAllocations > 0) {
            arg->pOpenAllocationInfo[0].hAllocation = getOpenResourceArgOut().pOpenAllocationInfo->hAllocation;
            arg->pOpenAllocationInfo[0].PrivateDriverDataSize = getOpenResourceArgOut().pOpenAllocationInfo->PrivateDriverDataSize;
            arg->pOpenAllocationInfo[0].pPrivateDriverData = getOpenResourceArgOut().pOpenAllocationInfo->pPrivateDriverData;
        }
        return 0;
    }

    bool getAllProcAddresses() override {
        makeResident = reinterpret_cast<PFND3DKMT_MAKERESIDENT>(makeResidentMock);
        if (nonZeroNumBytesToTrim) {
            evict = reinterpret_cast<PFND3DKMT_EVICT>(evictMockWithNonZeroTrim);
        } else {
            evict = reinterpret_cast<PFND3DKMT_EVICT>(evictMock);
        }
        registerTrimNotification = reinterpret_cast<PFND3DKMT_REGISTERTRIMNOTIFICATION>(registerTrimNotificationMock);
        unregisterTrimNotification = reinterpret_cast<PFND3DKMT_UNREGISTERTRIMNOTIFICATION>(unregisterTrimNotificationMock);
        destroyAllocation2 = reinterpret_cast<PFND3DKMT_DESTROYALLOCATION2>(destroyAllocation2Mock);
        waitForSynchronizationObjectFromCpu = reinterpret_cast<PFND3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU>(waitFromCpuMock);
        queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(queryResourceInfoMock);
        openResource = reinterpret_cast<PFND3DKMT_OPENRESOURCE>(openResourceMock);
        return true;
    }

    static D3DDDI_MAKERESIDENT &getMakeResidentArg() {
        static D3DDDI_MAKERESIDENT makeResidentArg;
        return makeResidentArg;
    }

    static D3DKMT_EVICT &getEvictArg() {
        static D3DKMT_EVICT evictArg;
        return evictArg;
    }

    static D3DKMT_REGISTERTRIMNOTIFICATION &getRegisterTrimNotificationArg() {
        static D3DKMT_REGISTERTRIMNOTIFICATION registerTrimArg;
        return registerTrimArg;
    }

    static D3DKMT_UNREGISTERTRIMNOTIFICATION &getUnregisterTrimNotificationArg() {
        static D3DKMT_UNREGISTERTRIMNOTIFICATION unregisterTrimArg;
        return unregisterTrimArg;
    }

    static D3DKMT_DESTROYALLOCATION2 &getDestroyArg() {
        static D3DKMT_DESTROYALLOCATION2 destroyArg;
        return destroyArg;
    }

    static D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU &getWaitFromCpuArg() {
        static D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU waitFromCpu;
        return waitFromCpu;
    }

    static D3DKMT_CREATESYNCHRONIZATIONOBJECT2 getCreateSynchronizationObject2Arg() {
        static D3DKMT_CREATESYNCHRONIZATIONOBJECT2 createSynchronizationObject2;
        return createSynchronizationObject2;
    }

    static D3DKMT_QUERYRESOURCEINFO &getQueryResourceInfoArgIn() {
        static D3DKMT_QUERYRESOURCEINFO queryResourceInfo;
        return queryResourceInfo;
    }

    static D3DKMT_QUERYRESOURCEINFO &getQueryResourceInfoArgOut() {
        static D3DKMT_QUERYRESOURCEINFO queryResourceInfo;
        return queryResourceInfo;
    }

    static D3DKMT_OPENRESOURCE &getOpenResourceArgIn() {
        static D3DKMT_OPENRESOURCE openResource;
        return openResource;
    }

    static D3DKMT_OPENRESOURCE &getOpenResourceArgOut() {
        static D3DKMT_OPENRESOURCE openResource;
        return openResource;
    }
};

} // namespace NEO
