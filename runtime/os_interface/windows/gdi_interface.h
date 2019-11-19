/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/windows/os_library_win.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/windows/os_inc.h"
#include "runtime/os_interface/windows/thk_wrapper.h"

#include <d3d9types.h>

#include <d3dkmthk.h>

#include <string>

namespace NEO {

class Gdi {
  public:
    Gdi();
    ~Gdi(){};

    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_OPENADAPTERFROMHDC *> openAdapterFromHdc;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_OPENADAPTERFROMLUID *> openAdapterFromLuid;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_CREATEALLOCATION *> createAllocation;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_DESTROYALLOCATION *> destroyAllocation;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_DESTROYALLOCATION2 *> destroyAllocation2;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_QUERYADAPTERINFO *> queryAdapterInfo;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_CLOSEADAPTER *> closeAdapter;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_CREATEDEVICE *> createDevice;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_DESTROYDEVICE *> destroyDevice;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_ESCAPE *> escape;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN D3DKMT_CREATECONTEXTVIRTUAL *> createContext;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_DESTROYCONTEXT *> destroyContext;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_OPENRESOURCE *> openResource;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_OPENRESOURCEFROMNTHANDLE *> openResourceFromNtHandle;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_QUERYRESOURCEINFO *> queryResourceInfo;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *> queryResourceInfoFromNtHandle;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_LOCK *> lock;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_UNLOCK *> unlock;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_RENDER *> render;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT *> createSynchronizationObject;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *> createSynchronizationObject2;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *> destroySynchronizationObject;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *> signalSynchronizationObject;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *> waitForSynchronizationObject;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *> waitForSynchronizationObjectFromCpu;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *> signalSynchronizationObjectFromCpu;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *> waitForSynchronizationObjectFromGpu;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *> signalSynchronizationObjectFromGpu;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_CREATEPAGINGQUEUE *> createPagingQueue;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DDDI_DESTROYPAGINGQUEUE *> destroyPagingQueue;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_LOCK2 *> lock2;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_UNLOCK2 *> unlock2;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DDDI_MAPGPUVIRTUALADDRESS *> mapGpuVirtualAddress;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DDDI_RESERVEGPUVIRTUALADDRESS *> reserveGpuVirtualAddress;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_FREEGPUVIRTUALADDRESS *> freeGpuVirtualAddress;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *> updateGpuVirtualAddress;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_SUBMITCOMMAND *> submitCommand;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DDDI_MAKERESIDENT *> makeResident;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN D3DKMT_EVICT *> evict;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN D3DKMT_REGISTERTRIMNOTIFICATION *> registerTrimNotification;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN D3DKMT_UNREGISTERTRIMNOTIFICATION *> unregisterTrimNotification;

    // HW queue
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_CREATEHWQUEUE *> createHwQueue;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_DESTROYHWQUEUE *> destroyHwQueue;
    ThkWrapper<OCL_RUNTIME_PROFILING, IN CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *> submitCommandToHwQueue;

    // For debug purposes
    ThkWrapper<OCL_RUNTIME_PROFILING, IN OUT D3DKMT_GETDEVICESTATE *> getDeviceState;

    bool isInitialized() {
        return initialized;
    }

    MOCKABLE_VIRTUAL bool setupHwQueueProcAddresses();

  protected:
    MOCKABLE_VIRTUAL bool getAllProcAddresses();
    bool initialized;
    NEO::Windows::OsLibrary gdiDll;
};
} // namespace NEO
