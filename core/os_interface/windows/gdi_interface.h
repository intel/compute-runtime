/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/options.h"
#include "core/os_interface/windows/os_inc.h"
#include "core/os_interface/windows/os_library_win.h"
#include "core/os_interface/windows/thk_wrapper.h"

#include <d3d9types.h>

#include <d3dkmthk.h>

#include <string>

namespace NEO {

class Gdi {
  public:
    Gdi();
    ~Gdi(){};

    ThkWrapper<IN OUT D3DKMT_OPENADAPTERFROMHDC *> openAdapterFromHdc;
    ThkWrapper<IN OUT D3DKMT_OPENADAPTERFROMLUID *> openAdapterFromLuid;
    ThkWrapper<IN OUT D3DKMT_CREATEALLOCATION *> createAllocation;
    ThkWrapper<IN CONST D3DKMT_DESTROYALLOCATION *> destroyAllocation;
    ThkWrapper<IN CONST D3DKMT_DESTROYALLOCATION2 *> destroyAllocation2;
    ThkWrapper<IN CONST D3DKMT_QUERYADAPTERINFO *> queryAdapterInfo;
    ThkWrapper<IN CONST D3DKMT_CLOSEADAPTER *> closeAdapter;
    ThkWrapper<IN OUT D3DKMT_CREATEDEVICE *> createDevice;
    ThkWrapper<IN CONST D3DKMT_DESTROYDEVICE *> destroyDevice;
    ThkWrapper<IN CONST D3DKMT_ESCAPE *> escape;
    ThkWrapper<IN D3DKMT_CREATECONTEXTVIRTUAL *> createContext;
    ThkWrapper<IN CONST D3DKMT_DESTROYCONTEXT *> destroyContext;
    ThkWrapper<IN OUT D3DKMT_OPENRESOURCE *> openResource;
    ThkWrapper<IN OUT D3DKMT_OPENRESOURCEFROMNTHANDLE *> openResourceFromNtHandle;
    ThkWrapper<IN OUT D3DKMT_QUERYRESOURCEINFO *> queryResourceInfo;
    ThkWrapper<IN OUT D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *> queryResourceInfoFromNtHandle;
    ThkWrapper<IN OUT D3DKMT_LOCK *> lock;
    ThkWrapper<IN CONST D3DKMT_UNLOCK *> unlock;
    ThkWrapper<IN OUT D3DKMT_RENDER *> render;
    ThkWrapper<IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT *> createSynchronizationObject;
    ThkWrapper<IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *> createSynchronizationObject2;
    ThkWrapper<IN CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *> destroySynchronizationObject;
    ThkWrapper<IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *> signalSynchronizationObject;
    ThkWrapper<IN CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *> waitForSynchronizationObject;
    ThkWrapper<IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *> waitForSynchronizationObjectFromCpu;
    ThkWrapper<IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *> signalSynchronizationObjectFromCpu;
    ThkWrapper<IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *> waitForSynchronizationObjectFromGpu;
    ThkWrapper<IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *> signalSynchronizationObjectFromGpu;
    ThkWrapper<IN OUT D3DKMT_CREATEPAGINGQUEUE *> createPagingQueue;
    ThkWrapper<IN OUT D3DDDI_DESTROYPAGINGQUEUE *> destroyPagingQueue;
    ThkWrapper<IN OUT D3DKMT_LOCK2 *> lock2;
    ThkWrapper<IN CONST D3DKMT_UNLOCK2 *> unlock2;
    ThkWrapper<IN OUT D3DDDI_MAPGPUVIRTUALADDRESS *> mapGpuVirtualAddress;
    ThkWrapper<IN OUT D3DDDI_RESERVEGPUVIRTUALADDRESS *> reserveGpuVirtualAddress;
    ThkWrapper<IN CONST D3DKMT_FREEGPUVIRTUALADDRESS *> freeGpuVirtualAddress;
    ThkWrapper<IN CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *> updateGpuVirtualAddress;
    ThkWrapper<IN CONST D3DKMT_SUBMITCOMMAND *> submitCommand;
    ThkWrapper<IN OUT D3DDDI_MAKERESIDENT *> makeResident;
    ThkWrapper<IN D3DKMT_EVICT *> evict;
    ThkWrapper<IN D3DKMT_REGISTERTRIMNOTIFICATION *> registerTrimNotification;
    ThkWrapper<IN D3DKMT_UNREGISTERTRIMNOTIFICATION *> unregisterTrimNotification;

    // HW queue
    ThkWrapper<IN OUT D3DKMT_CREATEHWQUEUE *> createHwQueue;
    ThkWrapper<IN CONST D3DKMT_DESTROYHWQUEUE *> destroyHwQueue;
    ThkWrapper<IN CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *> submitCommandToHwQueue;

    // For debug purposes
    ThkWrapper<IN OUT D3DKMT_GETDEVICESTATE *> getDeviceState;

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
