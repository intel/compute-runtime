/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once
#include "runtime/os_interface/windows/gdi_interface.h"

namespace OCLRT {

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

    static NTSTATUS __stdcall registerTimNotificationMock(IN D3DKMT_REGISTERTRIMNOTIFICATION *arg) {
        getRegisterTrimNotificationArg() = *arg;
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
        registerTrimNotification = reinterpret_cast<PFND3DKMT_REGISTERTRIMNOTIFICATION>(registerTimNotificationMock);
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

} // namespace OCLRT
