/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"

#include <cwchar>
#include <dxcore.h>
#include <dxgi.h>

namespace NEO {
constexpr auto error = 1;

class UltDxCoreAdapter : public IDXCoreAdapter {
  public:
    const static char *description;
    LUID luid = {0u, 0x1234u};
    bool STDMETHODCALLTYPE IsValid() override {
        return true;
    }

    bool STDMETHODCALLTYPE IsAttributeSupported(REFGUID attributeGUID) override {
        UNRECOVERABLE_IF(true);
        return false;
    }

    bool STDMETHODCALLTYPE IsPropertySupported(DXCoreAdapterProperty property) override {
        UNRECOVERABLE_IF(true);
        return false;
    }

    HRESULT STDMETHODCALLTYPE GetProperty(DXCoreAdapterProperty property, size_t bufferSize,
                                          _Out_writes_bytes_(bufferSize) void *propertyData) override {
        size_t requiredSize;
        GetPropertySize(property, &requiredSize);
        if (bufferSize < requiredSize) {
            return error;
        }
        switch (property) {
        default:
            UNRECOVERABLE_IF(true);
            return error;
        case DXCoreAdapterProperty::IsHardware:
            *reinterpret_cast<bool *>(propertyData) = true;
            break;
        case DXCoreAdapterProperty::DriverDescription:
            memcpy_s(propertyData, bufferSize, description, requiredSize);
            break;
        case DXCoreAdapterProperty::InstanceLuid:
            reinterpret_cast<LUID *>(propertyData)->HighPart = luid.HighPart;
            reinterpret_cast<LUID *>(propertyData)->LowPart = luid.LowPart;
            break;
        case DXCoreAdapterProperty::HardwareID: {
            DXCoreHardwareID ret = {};
            ret.deviceID = 0x1234;
            *reinterpret_cast<DXCoreHardwareID *>(propertyData) = ret;
        } break;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertySize(DXCoreAdapterProperty property, _Out_ size_t *bufferSize) override {
        switch (property) {
        default:
            UNRECOVERABLE_IF(true);
            return error;
        case DXCoreAdapterProperty::IsHardware:
            *bufferSize = sizeof(bool);
            break;
        case DXCoreAdapterProperty::DriverDescription:
            *bufferSize = strlen(description) + 1;
            break;
        case DXCoreAdapterProperty::InstanceLuid:
            *bufferSize = sizeof(LUID);
            break;
        case DXCoreAdapterProperty::HardwareID:
            *bufferSize = sizeof(DXCoreHardwareID);
            break;
        }

        return S_OK;
    }

    bool STDMETHODCALLTYPE IsQueryStateSupported(DXCoreAdapterState property) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    HRESULT STDMETHODCALLTYPE QueryState(DXCoreAdapterState state, size_t inputStateDetailsSize,
                                         _In_reads_bytes_opt_(inputStateDetailsSize) const void *inputStateDetails,
                                         size_t outputBufferSize, _Out_writes_bytes_(outputBufferSize) void *outputBuffer) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    bool STDMETHODCALLTYPE IsSetStateSupported(DXCoreAdapterState property) override {
        UNRECOVERABLE_IF(true);
        return false;
    }

    HRESULT STDMETHODCALLTYPE SetState(DXCoreAdapterState state, size_t inputStateDetailsSize,
                                       _In_reads_bytes_opt_(inputStateDetailsSize) const void *inputStateDetails,
                                       size_t inputDataSize, _In_reads_bytes_(inputDataSize) const void *inputData) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    HRESULT STDMETHODCALLTYPE GetFactory(REFIID riid, _COM_Outptr_ void **ppvFactory) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) override {
        UNRECOVERABLE_IF(true);
        return 0;
    }

    ULONG STDMETHODCALLTYPE Release(void) override {
        // this must be the last instruction
        delete this;
        return 0;
    }
};

extern uint32_t numRootDevicesToEnum;
class UltDXCoreAdapterList : public IDXCoreAdapterList {
  public:
    static bool firstInvalid;
    HRESULT STDMETHODCALLTYPE GetAdapter(uint32_t index, REFIID riid, _COM_Outptr_ void **ppvAdapter) override {
        auto adapter = new UltDxCoreAdapter;
        if (firstInvalid && 0 == index) {
            adapter->luid.HighPart = 0u;
            adapter->luid.LowPart = 0u;
        }

        *reinterpret_cast<UltDxCoreAdapter **>(ppvAdapter) = adapter;
        return S_OK;
    }
    uint32_t STDMETHODCALLTYPE GetAdapterCount() override {
        return numRootDevicesToEnum;
    }

    bool STDMETHODCALLTYPE IsStale() override {
        return false;
    }

    HRESULT STDMETHODCALLTYPE GetFactory(REFIID riid, _COM_Outptr_ void **ppvFactory) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    HRESULT STDMETHODCALLTYPE Sort(uint32_t numPreferences, _In_reads_(numPreferences) const DXCoreAdapterPreference *preferences) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    bool STDMETHODCALLTYPE IsAdapterPreferenceSupported(DXCoreAdapterPreference preference) override {
        UNRECOVERABLE_IF(true);
        return false;
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) override {
        UNRECOVERABLE_IF(true);
        return 0;
    }

    ULONG STDMETHODCALLTYPE Release(void) override {
        // this must be the last instruction
        delete this;
        return 0;
    }
};

extern uint32_t numRootDevicesToEnum;
class UltDXCoreAdapterFactory : public IDXCoreAdapterFactory {
  public:
    struct CreateAdapterListArgs {
        uint32_t numAttributes;
        const GUID *filterAttributesPtr;
        std::vector<const GUID *> filterAttributesCopy;
        REFIID riid;
        void **ppvAdapterList;
    };

    std::vector<CreateAdapterListArgs> argsOfCreateAdapterListRequests;

    HRESULT STDMETHODCALLTYPE CreateAdapterList(uint32_t numAttributes, _In_reads_(numAttributes) const GUID *filterAttributes,
                                                REFIID riid, _COM_Outptr_ void **ppvAdapterList) override {
        argsOfCreateAdapterListRequests.push_back({numAttributes,
                                                   filterAttributes,
                                                   std::vector<const GUID *>{filterAttributes, filterAttributes + numAttributes},
                                                   riid, ppvAdapterList});

        *reinterpret_cast<UltDXCoreAdapterList **>(ppvAdapterList) = new UltDXCoreAdapterList;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetAdapterByLuid(const LUID &adapterLUID, REFIID riid, _COM_Outptr_ void **ppvAdapter) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    bool STDMETHODCALLTYPE IsNotificationTypeSupported(DXCoreNotificationType notificationType) override {
        UNRECOVERABLE_IF(true);
        return false;
    }

    HRESULT STDMETHODCALLTYPE RegisterEventNotification(_In_ IUnknown *dxCoreObject, DXCoreNotificationType notificationType,
                                                        _In_ PFN_DXCORE_NOTIFICATION_CALLBACK callbackFunction,
                                                        _In_opt_ void *callbackContext, _Out_ uint32_t *eventCookie) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    HRESULT STDMETHODCALLTYPE UnregisterEventNotification(uint32_t eventCookie) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject) override {
        UNRECOVERABLE_IF(true);
        return error;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) override {
        UNRECOVERABLE_IF(true);
        return 0;
    }

    ULONG STDMETHODCALLTYPE Release(void) override {
        // this must be the last instruction
        delete this;
        return 0;
    }
};

HRESULT WINAPI ULTDXCoreCreateAdapterFactory(REFIID riid, void **ppFactory);
void WINAPI ULTGetSystemInfo(SYSTEM_INFO *pSystemInfo);

} // namespace NEO
