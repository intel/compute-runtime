/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"

#include <cwchar>
#include <dxgi.h>

namespace NEO {

struct UltIDXGIOutput : public IDXGIOutput {

    UltIDXGIOutput(uint32_t id) : id(id) {}
    uint32_t id = 0;

    HRESULT STDMETHODCALLTYPE GetDesc(_Out_ DXGI_OUTPUT_DESC *pDesc) override {

        if (id == 0) {
            WCHAR deviceName[] = L"Display0";
            wcscpy_s(pDesc->DeviceName, ARRAYSIZE(deviceName), deviceName);
        } else if (id == 1) {
            WCHAR deviceName[] = L"Display1";
            wcscpy_s(pDesc->DeviceName, ARRAYSIZE(deviceName), deviceName);
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDisplayModeList(
        /* [in] */ DXGI_FORMAT EnumFormat,
        /* [in] */ UINT Flags,
        /* [annotation][out][in] */
        _Inout_ UINT *pNumModes,
        /* [annotation][out] */
        _Out_writes_to_opt_(*pNumModes, *pNumModes) DXGI_MODE_DESC *pDesc) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(
        /* [annotation][in] */
        _In_ const DXGI_MODE_DESC *pModeToMatch,
        /* [annotation][out] */
        _Out_ DXGI_MODE_DESC *pClosestMatch,
        /* [annotation][in] */
        _In_opt_ IUnknown *pConcernedDevice) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE WaitForVBlank(void) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE TakeOwnership(
        /* [annotation][in] */
        _In_ IUnknown *pDevice,
        BOOL Exclusive) override { return S_OK; }

    void STDMETHODCALLTYPE ReleaseOwnership(void) override {}

    HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(
        /* [annotation][out] */
        _Out_ DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE SetGammaControl(
        /* [annotation][in] */
        _In_ const DXGI_GAMMA_CONTROL *pArray) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetGammaControl(
        /* [annotation][out] */
        _Out_ DXGI_GAMMA_CONTROL *pArray) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE SetDisplaySurface(
        /* [annotation][in] */
        _In_ IDXGISurface *pScanoutSurface) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(
        /* [annotation][in] */
        _In_ IDXGISurface *pDestination) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
        /* [annotation][out] */
        _Out_ DXGI_FRAME_STATISTICS *pStats) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE SetPrivateData(
        /* [annotation][in] */
        _In_ REFGUID Name,
        /* [in] */ UINT DataSize,
        /* [annotation][in] */
        _In_reads_bytes_(DataSize) const void *pData) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        /* [annotation][in] */
        _In_ REFGUID Name,
        /* [annotation][in] */
        _In_opt_ const IUnknown *pUnknown) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetPrivateData(
        /* [annotation][in] */
        _In_ REFGUID Name,
        /* [annotation][out][in] */
        _Inout_ UINT *pDataSize,
        /* [annotation][out] */
        _Out_writes_bytes_(*pDataSize) void *pData) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetParent(
        /* [annotation][in] */
        _In_ REFIID riid,
        /* [annotation][retval][out] */
        _COM_Outptr_ void **ppParent) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE QueryInterface(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override { return S_OK; }

    ULONG STDMETHODCALLTYPE AddRef(void) override { return S_OK; }

    ULONG STDMETHODCALLTYPE Release(void) override { return S_OK; }
};
class UltIDXGIAdapter1 : public IDXGIAdapter1 {
  public:
    uint32_t id = 0u;
    UltIDXGIAdapter1(uint32_t id) : id(id), output{id} {};
    const static wchar_t *description;
    UltIDXGIOutput output;
    // IDXGIAdapter1
    HRESULT STDMETHODCALLTYPE GetDesc1(
        _Out_ DXGI_ADAPTER_DESC1 *pDesc) {

        if (pDesc == nullptr) {
            return S_FALSE;
        }
        swprintf(pDesc->Description, 128, description);
        pDesc->AdapterLuid.HighPart = 0x1234;
        pDesc->AdapterLuid.LowPart = id;
        pDesc->DeviceId = 0x1234;
        return S_OK;
    }

    // IDXGIAdapter
    HRESULT STDMETHODCALLTYPE EnumOutputs(
        UINT outputId,
        IDXGIOutput **ppOutput) {
        if (outputId == 0) {
            *ppOutput = &output;
        } else {
            *ppOutput = nullptr;
            return DXGI_ERROR_NOT_FOUND;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDesc(
        DXGI_ADAPTER_DESC *pDesc) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(
        _In_ REFGUID InterfaceName,
        _Out_ LARGE_INTEGER *pUMDVersion) {
        return S_OK;
    }

    // IDXGIObject
    HRESULT STDMETHODCALLTYPE SetPrivateData(
        _In_ REFGUID Name,
        UINT DataSize,
        const void *pData) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        _In_ REFGUID Name,
        _In_opt_ const IUnknown *pUnknown) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(
        _In_ REFGUID Name,
        _Inout_ UINT *pDataSize,
        _Out_writes_bytes_(*pDataSize) void *pData) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetParent(
        _In_ REFIID riid,
        _COM_Outptr_ void **ppParent) {
        return S_OK;
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        void __RPC_FAR *__RPC_FAR *ppvObject) {

        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) {
        return 0;
    }

    ULONG STDMETHODCALLTYPE Release(void) {
        // this must be the last instruction
        delete this;
        return 0;
    }
};

extern uint32_t numRootDevicesToEnum;
class UltIDXGIFactory1 : public IDXGIFactory1 {
  public:
    HRESULT STDMETHODCALLTYPE EnumAdapters1(
        UINT adapterId,
        IDXGIAdapter1 **ppAdapter) {
        if (adapterId >= numRootDevicesToEnum) {
            *(IDXGIAdapter1 **)ppAdapter = nullptr;
            return DXGI_ERROR_NOT_FOUND;
        }
        *(IDXGIAdapter1 **)ppAdapter = new UltIDXGIAdapter1(adapterId);
        return S_OK;
    }

    BOOL STDMETHODCALLTYPE IsCurrent(void) {
        return 0;
    }

    HRESULT STDMETHODCALLTYPE EnumAdapters(
        UINT Adapter,
        IDXGIAdapter **ppAdapter) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE MakeWindowAssociation(
        HWND WindowHandle,
        UINT Flags) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetWindowAssociation(
        _Out_ HWND *pWindowHandle) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CreateSwapChain(
        _In_ IUnknown *pDevice,
        _In_ DXGI_SWAP_CHAIN_DESC *pDesc,
        IDXGISwapChain **ppSwapChain) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(
        HMODULE Module,
        IDXGIAdapter **ppAdapter) {
        return S_OK;
    }

    // IDXGIObject
    HRESULT STDMETHODCALLTYPE SetPrivateData(
        _In_ REFGUID Name,
        UINT DataSize,
        const void *pData) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        _In_ REFGUID Name,
        _In_opt_ const IUnknown *pUnknown) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(
        _In_ REFGUID Name,
        _Inout_ UINT *pDataSize,
        _Out_writes_bytes_(*pDataSize) void *pData) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetParent(
        _In_ REFIID riid,
        _COM_Outptr_ void **ppParent) {
        return S_OK;
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        void __RPC_FAR *__RPC_FAR *ppvObject) {

        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) {
        return 0;
    }

    ULONG STDMETHODCALLTYPE Release(void) {
        // this must be the last instruction
        delete this;
        return 0;
    }
};
HRESULT WINAPI ULTCreateDXGIFactory(REFIID riid, void **ppFactory);
void WINAPI ULTGetSystemInfo(SYSTEM_INFO *pSystemInfo);

} // namespace NEO
