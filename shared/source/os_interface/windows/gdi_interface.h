/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/os_inc.h"
#include "shared/source/os_interface/windows/thk_wrapper.h"

#include <memory>
#include <string>

namespace NEO {

class Gdi {
  public:
    Gdi();
    MOCKABLE_VIRTUAL ~Gdi();

    ThkWrapper<IN OUT D3DKMT_OPENADAPTERFROMLUID *> openAdapterFromLuid{};
    ThkWrapper<IN OUT D3DKMT_CREATEALLOCATION *> createAllocation_{};
    ThkWrapper<IN OUT D3DKMT_CREATEALLOCATION *> createAllocation2{};
    NTSTATUS(APIENTRY *shareObjects)
    (UINT cObjects, const D3DKMT_HANDLE *hObjects, POBJECT_ATTRIBUTES pObjectAttributes, DWORD dwDesiredAccess, HANDLE *phSharedNtHandle) = {};
    ThkWrapper<IN CONST D3DKMT_DESTROYALLOCATION *> destroyAllocation{};
    ThkWrapper<IN CONST D3DKMT_DESTROYALLOCATION2 *> destroyAllocation2{};
    ThkWrapper<IN CONST D3DKMT_QUERYADAPTERINFO *> queryAdapterInfo{};
    ThkWrapper<IN CONST D3DKMT_CLOSEADAPTER *> closeAdapter{};
    ThkWrapper<IN OUT D3DKMT_CREATEDEVICE *> createDevice{};
    ThkWrapper<IN CONST D3DKMT_DESTROYDEVICE *> destroyDevice{};
    ThkWrapper<IN CONST D3DKMT_ESCAPE *> escape{};
    ThkWrapper<IN D3DKMT_CREATECONTEXTVIRTUAL *> createContext{};
    ThkWrapper<IN CONST D3DKMT_DESTROYCONTEXT *> destroyContext{};
    ThkWrapper<IN OUT D3DKMT_OPENRESOURCE *> openResource{};
    ThkWrapper<IN OUT D3DKMT_OPENRESOURCEFROMNTHANDLE *> openResourceFromNtHandle{};
    ThkWrapper<IN OUT D3DKMT_QUERYRESOURCEINFO *> queryResourceInfo{};
    ThkWrapper<IN OUT D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *> queryResourceInfoFromNtHandle{};
    ThkWrapper<IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT *> createSynchronizationObject{};
    ThkWrapper<IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *> createSynchronizationObject2{};
    ThkWrapper<IN CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *> destroySynchronizationObject{};
    ThkWrapper<IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *> signalSynchronizationObject{};
    ThkWrapper<IN CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *> waitForSynchronizationObject{};
    ThkWrapper<IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *> waitForSynchronizationObjectFromCpu{};
    ThkWrapper<IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *> signalSynchronizationObjectFromCpu{};
    ThkWrapper<IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *> waitForSynchronizationObjectFromGpu{};
    ThkWrapper<IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *> signalSynchronizationObjectFromGpu{};
    ThkWrapper<IN OUT D3DKMT_CREATEPAGINGQUEUE *> createPagingQueue{};
    ThkWrapper<IN OUT D3DDDI_DESTROYPAGINGQUEUE *> destroyPagingQueue{};
    ThkWrapper<IN OUT D3DKMT_LOCK2 *> lock2{};
    ThkWrapper<IN CONST D3DKMT_UNLOCK2 *> unlock2{};
    ThkWrapper<IN OUT D3DDDI_MAPGPUVIRTUALADDRESS *> mapGpuVirtualAddress{};
    ThkWrapper<IN OUT D3DDDI_RESERVEGPUVIRTUALADDRESS *> reserveGpuVirtualAddress{};
    ThkWrapper<IN CONST D3DKMT_FREEGPUVIRTUALADDRESS *> freeGpuVirtualAddress{};
    ThkWrapper<IN CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *> updateGpuVirtualAddress{};
    ThkWrapper<IN CONST D3DKMT_SUBMITCOMMAND *> submitCommand{};
    ThkWrapper<IN OUT D3DDDI_MAKERESIDENT *> makeResident{};
    ThkWrapper<IN D3DKMT_EVICT *> evict{};
    ThkWrapper<IN D3DKMT_REGISTERTRIMNOTIFICATION *> registerTrimNotification{};
    ThkWrapper<IN D3DKMT_UNREGISTERTRIMNOTIFICATION *> unregisterTrimNotification{};
    ThkWrapper<IN CONST D3DKMT_SETALLOCATIONPRIORITY *> setAllocationPriority{};
    ThkWrapper<IN CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *> setSchedulingPriority{};

    // HW queue
    ThkWrapper<IN OUT D3DKMT_CREATEHWQUEUE *> createHwQueue{};
    ThkWrapper<IN CONST D3DKMT_DESTROYHWQUEUE *> destroyHwQueue{};
    ThkWrapper<IN CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *> submitCommandToHwQueue{};

    // For debug purposes
    ThkWrapper<IN OUT D3DKMT_GETDEVICESTATE *> getDeviceState{};

    bool isInitialized() {
        return initialized;
    }

    MOCKABLE_VIRTUAL bool setupHwQueueProcAddresses();

  protected:
    MOCKABLE_VIRTUAL bool getAllProcAddresses();
    std::unique_ptr<NEO::OsLibrary> gdiDll;
    bool initialized = false;
};
} // namespace NEO
