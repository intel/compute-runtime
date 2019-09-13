/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/memory_manager/memory_constants.h"

#include <cwchar>
#include <dxgi.h>

namespace NEO {
class UltIDXGIAdapter1 : public IDXGIAdapter1 {
  public:
    // IDXGIAdapter1
    HRESULT STDMETHODCALLTYPE GetDesc1(
        _Out_ DXGI_ADAPTER_DESC1 *pDesc) {

        if (pDesc == nullptr) {
            return S_FALSE;
        }
        swprintf(pDesc->Description, 128, L"Intel");
        pDesc->AdapterLuid.HighPart = 0x1234;
        pDesc->DeviceId = 0x1234;
        return S_OK;
    }

    // IDXGIAdapter
    HRESULT STDMETHODCALLTYPE EnumOutputs(
        UINT Output,
        IDXGIOutput **ppOutput) {
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

class UltIDXGIFactory1 : public IDXGIFactory1 {
  public:
    HRESULT STDMETHODCALLTYPE EnumAdapters1(
        UINT Adapter,
        IDXGIAdapter1 **ppAdapter) {
        if (Adapter > 2) {
            *(IDXGIAdapter1 **)ppAdapter = nullptr;
            return DXGI_ERROR_NOT_FOUND;
        }
        *(IDXGIAdapter1 **)ppAdapter = new UltIDXGIAdapter1;
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
