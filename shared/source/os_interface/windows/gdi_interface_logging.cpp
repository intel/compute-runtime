/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface_logging.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/utilities/io_functions.h"

#include <cstdio>

namespace NEO {
namespace GdiLogging {

bool enabledLogging = false;

FILE *output = nullptr;

template <typename Param>
void getEnterString(Param param, char *input, size_t size);

template <typename Param>
void getExitString(Param param, char *input, size_t size) {
    input[0] = 0;
}

template <>
void getEnterString<CONST D3DKMT_OPENADAPTERFROMLUID *>(CONST D3DKMT_OPENADAPTERFROMLUID *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_OPENADAPTERFROMLUID LUID 0x%lx 0x%lx", param->AdapterLuid.HighPart, param->AdapterLuid.LowPart);
}

template <>
void getEnterString<CONST D3DKMT_CLOSEADAPTER *>(CONST D3DKMT_CLOSEADAPTER *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_CLOSEADAPTER");
}

template <>
void getEnterString<CONST D3DKMT_QUERYADAPTERINFO *>(CONST D3DKMT_QUERYADAPTERINFO *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_QUERYADAPTERINFO");
}

template <>
void getEnterString<CONST D3DKMT_ESCAPE *>(CONST D3DKMT_ESCAPE *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_ESCAPE Device 0x%x Type %u Flags 0x%x", param->hDevice, param->Type, param->Flags.Value);
}

template <>
void getEnterString<D3DKMT_CREATEDEVICE *>(D3DKMT_CREATEDEVICE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_CREATEDEVICE");
}

template <>
void getEnterString<CONST D3DKMT_DESTROYDEVICE *>(CONST D3DKMT_DESTROYDEVICE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_DESTROYDEVICE");
}

template <>
void getEnterString<D3DKMT_CREATECONTEXT *>(D3DKMT_CREATECONTEXT *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_CREATECONTEXT NodeOrdinal %u Flags 0x%x", param->NodeOrdinal, param->Flags.Value);
}

template <>
void getEnterString<CONST D3DKMT_DESTROYCONTEXT *>(CONST D3DKMT_DESTROYCONTEXT *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_DESTROYCONTEXT");
}

template <>
void getEnterString<D3DKMT_CREATEALLOCATION *>(D3DKMT_CREATEALLOCATION *param, char *input, size_t size) {
    static_assert(sizeof(UINT) == sizeof(D3DKMT_CREATEALLOCATIONFLAGS), "D3DKMT_CREATEALLOCATIONFLAGS is not UINT size");
    UINT flagsValue = 0;
    memcpy_s(&flagsValue, sizeof(flagsValue), &param->Flags, sizeof(param->Flags));
    snprintf_s(input, size, size, "D3DKMT_CREATEALLOCATION Flags 0x%x", flagsValue);
}

template <>
void getEnterString<D3DKMT_OPENRESOURCE *>(D3DKMT_OPENRESOURCE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_OPENRESOURCE");
}

template <>
void getEnterString<D3DKMT_QUERYRESOURCEINFO *>(D3DKMT_QUERYRESOURCEINFO *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_QUERYRESOURCEINFO");
}

template <>
void getEnterString<D3DKMT_CREATESYNCHRONIZATIONOBJECT *>(D3DKMT_CREATESYNCHRONIZATIONOBJECT *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_CREATESYNCHRONIZATIONOBJECT Info Type %u", param->Info.Type);
}

template <>
void getEnterString<CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *>(CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_DESTROYSYNCHRONIZATIONOBJECT");
}

template <>
void getEnterString<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *>(CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_SIGNALSYNCHRONIZATIONOBJECT Flags 0x%x", param->Flags.Value);
}

template <>
void getEnterString<CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *>(CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_WAITFORSYNCHRONIZATIONOBJECT");
}

template <>
void getEnterString<D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *>(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_CREATESYNCHRONIZATIONOBJECT2 Info Type %u Info Flags 0x%x", param->Info.Type, param->Info.Flags.Value);
}

template <>
void getEnterString<D3DKMT_GETDEVICESTATE *>(D3DKMT_GETDEVICESTATE *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_GETDEVICESTATE StateType %u", param->StateType);
}

template <>
void getEnterString<D3DDDI_MAKERESIDENT *>(D3DDDI_MAKERESIDENT *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DDDI_MAKERESIDENT NumAllocations %u Flags 0x%x", param->NumAllocations, param->Flags.Value);
}
template <>
void getExitString<D3DDDI_MAKERESIDENT *>(D3DDDI_MAKERESIDENT *param, char *input, size_t size) {
    snprintf_s(input, size, size, "PagingFenceValue %llu NumBytesToTrim 0x%llx", param->PagingFenceValue, param->NumBytesToTrim);
}

template <>
void getEnterString<D3DKMT_EVICT *>(D3DKMT_EVICT *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_EVICT NumAllocations %u Flags 0x%x", param->NumAllocations, param->Flags.Value);
}
template <>
void getExitString<D3DKMT_EVICT *>(D3DKMT_EVICT *param, char *input, size_t size) {
    snprintf_s(input, size, size, "NumBytesToTrim 0x%llx", param->NumBytesToTrim, param->Flags.Value);
}

template <>
void getEnterString<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *>(CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU Flags 0x%x", param->Flags.Value);
}

template <>
void getEnterString<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *>(CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU Flags 0x%x", param->Flags.Value);
}

template <>
void getEnterString<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *>(CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU");
}

template <>
void getEnterString<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *>(CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU");
}

template <>
void getEnterString<D3DKMT_CREATEPAGINGQUEUE *>(D3DKMT_CREATEPAGINGQUEUE *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_CREATEPAGINGQUEUE Priority %d", param->Priority);
}

template <>
void getEnterString<D3DDDI_DESTROYPAGINGQUEUE *>(D3DDDI_DESTROYPAGINGQUEUE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DDDI_DESTROYPAGINGQUEUE");
}

template <>
void getEnterString<D3DKMT_LOCK2 *>(D3DKMT_LOCK2 *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_LOCK2 Flags 0x%x", param->Flags.Value);
}

template <>
void getEnterString<CONST D3DKMT_UNLOCK2 *>(CONST D3DKMT_UNLOCK2 *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_UNLOCK2");
}

template <>
void getEnterString<CONST D3DKMT_INVALIDATECACHE *>(CONST D3DKMT_INVALIDATECACHE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_INVALIDATECACHE");
}

template <>
void getEnterString<D3DDDI_MAPGPUVIRTUALADDRESS *>(D3DDDI_MAPGPUVIRTUALADDRESS *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DDDI_MAPGPUVIRTUALADDRESS BaseAddress 0x%llx MinimumAddress 0x%llx MaximumAddress 0x%llx SizeInPages 0x%llx",
               param->BaseAddress, param->MinimumAddress, param->MaximumAddress, param->SizeInPages);
}
template <>
void getExitString<D3DDDI_MAPGPUVIRTUALADDRESS *>(D3DDDI_MAPGPUVIRTUALADDRESS *param, char *input, size_t size) {
    snprintf_s(input, size, size, "VirtualAddress 0x%llx PagingFenceValue %lu",
               param->VirtualAddress, param->PagingFenceValue);
}

template <>
void getEnterString<D3DDDI_RESERVEGPUVIRTUALADDRESS *>(D3DDDI_RESERVEGPUVIRTUALADDRESS *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DDDI_RESERVEGPUVIRTUALADDRESS BaseAddress 0x%llx MinimumAddress 0x%llx MaximumAddress 0x%llx Size 0x%llx ReservationType %u",
               param->BaseAddress, param->MinimumAddress, param->MaximumAddress, param->Size, param->ReservationType);
}
template <>
void getExitString<D3DDDI_RESERVEGPUVIRTUALADDRESS *>(D3DDDI_RESERVEGPUVIRTUALADDRESS *param, char *input, size_t size) {
    snprintf_s(input, size, size, "VirtualAddress 0x%llx PagingFenceValue %lu",
               param->VirtualAddress, param->PagingFenceValue);
}

template <>
void getEnterString<CONST D3DKMT_FREEGPUVIRTUALADDRESS *>(CONST D3DKMT_FREEGPUVIRTUALADDRESS *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_FREEGPUVIRTUALADDRESS BaseAddress 0x%llx Size 0x%llx", param->BaseAddress, param->Size);
}

template <>
void getEnterString<CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *>(CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_UPDATEGPUVIRTUALADDRESS");
}

template <>
void getEnterString<D3DKMT_CREATECONTEXTVIRTUAL *>(D3DKMT_CREATECONTEXTVIRTUAL *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_CREATECONTEXTVIRTUAL NodeOrdinal %u Flags 0x%x ClientHint %u", param->NodeOrdinal, param->Flags.Value, param->ClientHint);
}

template <>
void getEnterString<CONST D3DKMT_SUBMITCOMMAND *>(CONST D3DKMT_SUBMITCOMMAND *param, char *input, size_t size) {
    static_assert(sizeof(UINT) == sizeof(D3DKMT_SUBMITCOMMANDFLAGS), "D3DKMT_SUBMITCOMMANDFLAGS is not UINT size");
    UINT flagsValue = 0;
    memcpy_s(&flagsValue, sizeof(flagsValue), &param->Flags, sizeof(param->Flags));
    auto cmdBufferHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(param->pPrivateDriverData);
    snprintf_s(input, size, size, "D3DKMT_SUBMITCOMMAND Commands 0x%llx CommandLength 0x%x Flags 0x%x Header MonitorFenceValue 0x%llx",
               param->Commands, param->CommandLength, flagsValue, cmdBufferHeader->MonitorFenceValue);
}

template <>
void getEnterString<D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *>(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 Flags 0x%x", param->Flags.Value);
}

template <>
void getEnterString<D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *>(D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME");
}

template <>
void getEnterString<CONST D3DKMT_DESTROYALLOCATION2 *>(CONST D3DKMT_DESTROYALLOCATION2 *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_DESTROYALLOCATION2 Flags 0x%x", param->Flags.Value);
}

template <>
void getEnterString<D3DKMT_REGISTERTRIMNOTIFICATION *>(D3DKMT_REGISTERTRIMNOTIFICATION *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_REGISTERTRIMNOTIFICATION");
}

template <>
void getEnterString<D3DKMT_UNREGISTERTRIMNOTIFICATION *>(D3DKMT_UNREGISTERTRIMNOTIFICATION *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_UNREGISTERTRIMNOTIFICATION");
}

template <>
void getEnterString<D3DKMT_OPENRESOURCEFROMNTHANDLE *>(D3DKMT_OPENRESOURCEFROMNTHANDLE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_OPENRESOURCEFROMNTHANDLE");
}

template <>
void getEnterString<D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *>(D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE");
}

template <>
void getEnterString<D3DKMT_CREATEHWQUEUE *>(D3DKMT_CREATEHWQUEUE *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_CREATEHWQUEUE Flags 0x%x", param->Flags.Value);
}

template <>
void getEnterString<CONST D3DKMT_DESTROYHWQUEUE *>(CONST D3DKMT_DESTROYHWQUEUE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_DESTROYHWQUEUE");
}

template <>
void getEnterString<CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *>(CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_SUBMITCOMMANDTOHWQUEUE");
}

template <>
void getEnterString<CONST D3DKMT_SETALLOCATIONPRIORITY *>(CONST D3DKMT_SETALLOCATIONPRIORITY *param, char *input, size_t size) {
    strcpy_s(input, size, "D3DKMT_SETALLOCATIONPRIORITY");
}

template <>
void getEnterString<CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *>(CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_SETCONTEXTSCHEDULINGPRIORITY Priority %d", param->Priority);
}

template <typename Param>
inline void logEnter(Param param) {
    if (enabledLogging) {
        constexpr size_t stringBufferSize = 256;
        char stringBuffer[stringBufferSize];
        getEnterString<Param>(param, stringBuffer, stringBufferSize);
        IoFunctions::fprintf(output, "GDI Call ENTER %s\n", stringBuffer);
    }
}

template <typename Param>
inline void logExit(NTSTATUS status, Param param) {
    if (enabledLogging) {
        constexpr size_t stringBufferSize = 256;
        char stringBuffer[stringBufferSize];
        getExitString<Param>(param, stringBuffer, stringBufferSize);
        IoFunctions::fprintf(output, "GDI Call EXIT STATUS: 0x%x %s\n", status, stringBuffer);
    }
}

void init() {
    enabledLogging = debugManager.flags.LogGdiCalls.get();
    if (enabledLogging) {
        if (debugManager.flags.LogGdiCallsToFile.get()) {
            output = IoFunctions::fopenPtr("gdi.log", "rw");
        } else {
            output = stdout;
        }
    }
}

void close() {
    if (enabledLogging) {
        if (debugManager.flags.LogGdiCallsToFile.get()) {
            IoFunctions::fclosePtr(output);
        }
        enabledLogging = false;
        output = nullptr;
    }
}

template void logEnter<CONST D3DKMT_OPENADAPTERFROMLUID *>(CONST D3DKMT_OPENADAPTERFROMLUID *param);
template void logExit<CONST D3DKMT_OPENADAPTERFROMLUID *>(NTSTATUS status, CONST D3DKMT_OPENADAPTERFROMLUID *param);

template void logEnter<CONST D3DKMT_CLOSEADAPTER *>(CONST D3DKMT_CLOSEADAPTER *param);
template void logExit<CONST D3DKMT_CLOSEADAPTER *>(NTSTATUS status, CONST D3DKMT_CLOSEADAPTER *param);

template void logEnter<CONST D3DKMT_QUERYADAPTERINFO *>(CONST D3DKMT_QUERYADAPTERINFO *param);
template void logExit<CONST D3DKMT_QUERYADAPTERINFO *>(NTSTATUS status, CONST D3DKMT_QUERYADAPTERINFO *param);

template void logEnter<CONST D3DKMT_ESCAPE *>(CONST D3DKMT_ESCAPE *param);
template void logExit<CONST D3DKMT_ESCAPE *>(NTSTATUS status, CONST D3DKMT_ESCAPE *param);

template void logEnter<D3DKMT_CREATEDEVICE *>(D3DKMT_CREATEDEVICE *param);
template void logExit<D3DKMT_CREATEDEVICE *>(NTSTATUS status, D3DKMT_CREATEDEVICE *param);

template void logEnter<CONST D3DKMT_DESTROYDEVICE *>(CONST D3DKMT_DESTROYDEVICE *param);
template void logExit<CONST D3DKMT_DESTROYDEVICE *>(NTSTATUS status, CONST D3DKMT_DESTROYDEVICE *param);

template void logEnter<D3DKMT_CREATECONTEXT *>(D3DKMT_CREATECONTEXT *param);
template void logExit<D3DKMT_CREATECONTEXT *>(NTSTATUS status, D3DKMT_CREATECONTEXT *param);

template void logEnter<CONST D3DKMT_DESTROYCONTEXT *>(CONST D3DKMT_DESTROYCONTEXT *param);
template void logExit<CONST D3DKMT_DESTROYCONTEXT *>(NTSTATUS status, CONST D3DKMT_DESTROYCONTEXT *param);

template void logEnter<D3DKMT_CREATEALLOCATION *>(D3DKMT_CREATEALLOCATION *param);
template void logExit<D3DKMT_CREATEALLOCATION *>(NTSTATUS status, D3DKMT_CREATEALLOCATION *param);

template void logEnter<D3DKMT_OPENRESOURCE *>(D3DKMT_OPENRESOURCE *param);
template void logExit<D3DKMT_OPENRESOURCE *>(NTSTATUS status, D3DKMT_OPENRESOURCE *param);

template void logEnter<D3DKMT_QUERYRESOURCEINFO *>(D3DKMT_QUERYRESOURCEINFO *param);
template void logExit<D3DKMT_QUERYRESOURCEINFO *>(NTSTATUS status, D3DKMT_QUERYRESOURCEINFO *param);

template void logEnter<D3DKMT_CREATESYNCHRONIZATIONOBJECT *>(D3DKMT_CREATESYNCHRONIZATIONOBJECT *param);
template void logExit<D3DKMT_CREATESYNCHRONIZATIONOBJECT *>(NTSTATUS status, D3DKMT_CREATESYNCHRONIZATIONOBJECT *param);

template void logEnter<CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *>(CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *param);
template void logExit<CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *>(NTSTATUS status, CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *param);

template void logEnter<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *>(CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *param);
template void logExit<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *>(NTSTATUS status, CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *param);

template void logEnter<CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *>(CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *param);
template void logExit<CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *>(NTSTATUS status, CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *param);

template void logEnter<D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *>(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *param);
template void logExit<D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *>(NTSTATUS status, D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *param);

template void logEnter<D3DKMT_GETDEVICESTATE *>(D3DKMT_GETDEVICESTATE *param);
template void logExit<D3DKMT_GETDEVICESTATE *>(NTSTATUS status, D3DKMT_GETDEVICESTATE *param);

template void logEnter<D3DDDI_MAKERESIDENT *>(D3DDDI_MAKERESIDENT *param);
template void logExit<D3DDDI_MAKERESIDENT *>(NTSTATUS status, D3DDDI_MAKERESIDENT *param);

template void logEnter<D3DKMT_EVICT *>(D3DKMT_EVICT *param);
template void logExit<D3DKMT_EVICT *>(NTSTATUS status, D3DKMT_EVICT *param);

template void logEnter<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *>(CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *param);
template void logExit<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *>(NTSTATUS status, CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *param);

template void logEnter<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *>(CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *param);
template void logExit<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *>(NTSTATUS status, CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *param);

template void logEnter<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *>(CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *param);
template void logExit<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *>(NTSTATUS status, CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *param);

template void logEnter<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *>(CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *param);
template void logExit<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *>(NTSTATUS status, CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *param);

template void logEnter<D3DKMT_CREATEPAGINGQUEUE *>(D3DKMT_CREATEPAGINGQUEUE *param);
template void logExit<D3DKMT_CREATEPAGINGQUEUE *>(NTSTATUS status, D3DKMT_CREATEPAGINGQUEUE *param);

template void logEnter<D3DDDI_DESTROYPAGINGQUEUE *>(D3DDDI_DESTROYPAGINGQUEUE *param);
template void logExit<D3DDDI_DESTROYPAGINGQUEUE *>(NTSTATUS status, D3DDDI_DESTROYPAGINGQUEUE *param);

template void logEnter<D3DKMT_LOCK2 *>(D3DKMT_LOCK2 *param);
template void logExit<D3DKMT_LOCK2 *>(NTSTATUS status, D3DKMT_LOCK2 *param);

template void logEnter<CONST D3DKMT_UNLOCK2 *>(CONST D3DKMT_UNLOCK2 *param);
template void logExit<CONST D3DKMT_UNLOCK2 *>(NTSTATUS status, CONST D3DKMT_UNLOCK2 *param);

template void logEnter<CONST D3DKMT_INVALIDATECACHE *>(CONST D3DKMT_INVALIDATECACHE *param);
template void logExit<CONST D3DKMT_INVALIDATECACHE *>(NTSTATUS status, CONST D3DKMT_INVALIDATECACHE *param);

template void logEnter<D3DDDI_MAPGPUVIRTUALADDRESS *>(D3DDDI_MAPGPUVIRTUALADDRESS *param);
template void logExit<D3DDDI_MAPGPUVIRTUALADDRESS *>(NTSTATUS status, D3DDDI_MAPGPUVIRTUALADDRESS *param);

template void logEnter<D3DDDI_RESERVEGPUVIRTUALADDRESS *>(D3DDDI_RESERVEGPUVIRTUALADDRESS *param);
template void logExit<D3DDDI_RESERVEGPUVIRTUALADDRESS *>(NTSTATUS status, D3DDDI_RESERVEGPUVIRTUALADDRESS *param);

template void logEnter<CONST D3DKMT_FREEGPUVIRTUALADDRESS *>(CONST D3DKMT_FREEGPUVIRTUALADDRESS *param);
template void logExit<CONST D3DKMT_FREEGPUVIRTUALADDRESS *>(NTSTATUS status, CONST D3DKMT_FREEGPUVIRTUALADDRESS *param);

template void logEnter<CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *>(CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *param);
template void logExit<CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *>(NTSTATUS status, CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *param);

template void logEnter<D3DKMT_CREATECONTEXTVIRTUAL *>(D3DKMT_CREATECONTEXTVIRTUAL *param);
template void logExit<D3DKMT_CREATECONTEXTVIRTUAL *>(NTSTATUS status, D3DKMT_CREATECONTEXTVIRTUAL *param);

template void logEnter<CONST D3DKMT_SUBMITCOMMAND *>(CONST D3DKMT_SUBMITCOMMAND *param);
template void logExit<CONST D3DKMT_SUBMITCOMMAND *>(NTSTATUS status, CONST D3DKMT_SUBMITCOMMAND *param);

template void logEnter<D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *>(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *param);
template void logExit<D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *>(NTSTATUS status, D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *param);

template void logEnter<D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *>(D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *param);
template void logExit<D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *>(NTSTATUS status, D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *param);

template void logEnter<CONST D3DKMT_DESTROYALLOCATION2 *>(CONST D3DKMT_DESTROYALLOCATION2 *param);
template void logExit<CONST D3DKMT_DESTROYALLOCATION2 *>(NTSTATUS status, CONST D3DKMT_DESTROYALLOCATION2 *param);

template void logEnter<D3DKMT_REGISTERTRIMNOTIFICATION *>(D3DKMT_REGISTERTRIMNOTIFICATION *param);
template void logExit<D3DKMT_REGISTERTRIMNOTIFICATION *>(NTSTATUS status, D3DKMT_REGISTERTRIMNOTIFICATION *param);

template void logEnter<D3DKMT_UNREGISTERTRIMNOTIFICATION *>(D3DKMT_UNREGISTERTRIMNOTIFICATION *param);
template void logExit<D3DKMT_UNREGISTERTRIMNOTIFICATION *>(NTSTATUS status, D3DKMT_UNREGISTERTRIMNOTIFICATION *param);

template void logEnter<D3DKMT_OPENRESOURCEFROMNTHANDLE *>(D3DKMT_OPENRESOURCEFROMNTHANDLE *param);
template void logExit<D3DKMT_OPENRESOURCEFROMNTHANDLE *>(NTSTATUS status, D3DKMT_OPENRESOURCEFROMNTHANDLE *param);

template void logEnter<D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *>(D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *param);
template void logExit<D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *>(NTSTATUS status, D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *param);

template void logEnter<D3DKMT_CREATEHWQUEUE *>(D3DKMT_CREATEHWQUEUE *param);
template void logExit<D3DKMT_CREATEHWQUEUE *>(NTSTATUS status, D3DKMT_CREATEHWQUEUE *param);

template void logEnter<CONST D3DKMT_DESTROYHWQUEUE *>(CONST D3DKMT_DESTROYHWQUEUE *param);
template void logExit<CONST D3DKMT_DESTROYHWQUEUE *>(NTSTATUS status, CONST D3DKMT_DESTROYHWQUEUE *param);

template void logEnter<CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *>(CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *param);
template void logExit<CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *>(NTSTATUS status, CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *param);

template void logEnter<CONST D3DKMT_SETALLOCATIONPRIORITY *>(CONST D3DKMT_SETALLOCATIONPRIORITY *param);
template void logExit<CONST D3DKMT_SETALLOCATIONPRIORITY *>(NTSTATUS status, CONST D3DKMT_SETALLOCATIONPRIORITY *param);

template void logEnter<CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *>(CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *param);
template void logExit<CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *>(NTSTATUS status, CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *param);

#include "gdi_interface_logging.inl"

} // namespace GdiLogging

} // namespace NEO
