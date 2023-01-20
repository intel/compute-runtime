/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/wsl_compute_helper/source/gmm_resource_info_accessor.h"
#include "shared/wsl_compute_helper/source/wsl_compute_helper.h"
#include "shared/wsl_compute_helper/source/wsl_compute_helper_types_tokens_structs.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((__visibility__("default")))
#endif

namespace NEO {
extern const char *wslComputeHelperLibNameToLoad;
}

constexpr const char *mockTokToStrAdapterString = "MOCK_ADAPTER_TOK_TO_STR";
constexpr uint32_t mockKmdVersionInfo = 0xabcdefbc;
constexpr uint16_t mockTokToStrDriverBuildNumber = 0xabcd;
constexpr uint32_t mockTokToStrProcessID = 0xabcdefbc;
constexpr uint64_t mockTokToStrHeapBase = 0xabcdefbc;

constexpr const char *mockStrToTokAdapterString = "MOCK_ADAPTER_STR_TO_TOK";
constexpr uint16_t mockStrToTokDriverBuildNumber = 0xbadc;
constexpr uint32_t mockStrToTokProcessID = 0xcdbaebfc;
constexpr uint64_t mockStrToTokHeapBase = 0xabcdefbc;

extern "C" {
EXPORT size_t CCONV getSizeRequiredForStruct(TOK structId) {
    switch (structId) {
    default:
        return 0;
    case TOK_S_ADAPTER_INFO:
        return sizeof(ADAPTER_INFO);
    case TOK_S_COMMAND_BUFFER_HEADER_REC:
        return sizeof(COMMAND_BUFFER_HEADER);
    case TOK_S_CREATECONTEXT_PVTDATA:
        return sizeof(CREATECONTEXT_PVTDATA);
    case TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT:
        return sizeof(GmmResourceInfoWinStruct);
    case TOK_S_GMM_GFX_PARTITIONING:
        return sizeof(GMM_GFX_PARTITIONING) + 4;
    }
    return 0;
}

EXPORT bool CCONV tokensToStruct(TOK structId, void *dst, size_t dstSizeInBytes, const TokenHeader *begin, const TokenHeader *end) {
    if (dstSizeInBytes < getSizeRequiredForStruct(structId)) {
        return false;
    }
    switch (structId) {
    default:
        return false;
    case TOK_S_ADAPTER_INFO: {
        auto adapterInfo = new (dst) ADAPTER_INFO{};
        return (0 == memcpy_s(&adapterInfo->KmdVersionInfo, sizeof(adapterInfo->KmdVersionInfo), &mockKmdVersionInfo, sizeof(mockKmdVersionInfo)));
    } break;
    case TOK_S_COMMAND_BUFFER_HEADER_REC: {
        auto cmdBufferHeader = new (dst) COMMAND_BUFFER_HEADER{};
        cmdBufferHeader->UmdContextType = mockTokToStrDriverBuildNumber >> 12;
        return true;
    } break;
    case TOK_S_CREATECONTEXT_PVTDATA: {
        auto createCtxData = new (dst) CREATECONTEXT_PVTDATA{};
        createCtxData->ProcessID = mockTokToStrProcessID;
        return true;
    } break;
    case TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT: {
        auto gmmResourceInfo = new (dst) GmmResourceInfoWinStruct{};
        gmmResourceInfo->GmmResourceInfoCommon.pPrivateData = mockTokToStrDriverBuildNumber;
        return true;
    } break;
    case TOK_S_GMM_GFX_PARTITIONING: {
        auto gfxPartitioning = new (dst) GMM_GFX_PARTITIONING{};
        gfxPartitioning->Heap32->Base = mockTokToStrHeapBase;
        return true;
    } break;
    }
    return true;
}

EXPORT size_t CCONV getSizeRequiredForTokens(TOK structId) {
    switch (structId) {
    default:
        return 0;
    case TOK_S_ADAPTER_INFO:
        return sizeof(TOKSTR__ADAPTER_INFO);
    case TOK_S_COMMAND_BUFFER_HEADER_REC:
        return sizeof(TOKSTR_COMMAND_BUFFER_HEADER_REC);
    case TOK_S_CREATECONTEXT_PVTDATA:
        return sizeof(TOKSTR__CREATECONTEXT_PVTDATA);
    case TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT:
        return sizeof(TOKSTR_GmmResourceInfoWinStruct);
    case TOK_S_GMM_GFX_PARTITIONING:
        return sizeof(TOKSTR___GMM_GFX_PARTITIONING);
    }

    return 0;
}

EXPORT bool CCONV structToTokens(TOK structId, TokenHeader *dst, size_t dstSizeInBytes, const void *src, size_t srcSizeInBytes) {
    if (dstSizeInBytes < getSizeRequiredForTokens(structId)) {
        return false;
    }

    if (srcSizeInBytes < getSizeRequiredForStruct(structId)) {
        return false;
    }

    switch (structId) {
    default:
        return false;
    case TOK_S_ADAPTER_INFO: {
        auto adapterInfo = new (dst) TOKSTR__ADAPTER_INFO{};
        return (0 == memcpy_s(adapterInfo->DeviceRegistryPath.getValue<char>(), adapterInfo->DeviceRegistryPath.getValueSizeInBytes(), mockStrToTokAdapterString, strlen(mockStrToTokAdapterString) + 1));
    } break;
    case TOK_S_COMMAND_BUFFER_HEADER_REC: {
        auto cmdBufferHeader = new (dst) TOKSTR_COMMAND_BUFFER_HEADER_REC{};
        cmdBufferHeader->MonitorFenceValue.setValue(mockStrToTokDriverBuildNumber);
        return true;
    } break;
    case TOK_S_CREATECONTEXT_PVTDATA: {
        auto createCtxData = new (dst) TOKSTR__CREATECONTEXT_PVTDATA{};
        createCtxData->ProcessID.setValue(mockStrToTokProcessID);
        return true;
    } break;
    case TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT: {
        auto gmmResourceInfo = new (dst) TOKSTR_GmmResourceInfoWinStruct{};
        gmmResourceInfo->GmmResourceInfoCommon.pPrivateData.setValue(mockStrToTokDriverBuildNumber);
        return true;
    } break;
    case TOK_S_GMM_GFX_PARTITIONING: {
        auto gfxPartitioning = new (dst) TOKSTR___GMM_GFX_PARTITIONING{};
        gfxPartitioning->Heap32->Base.setValue(mockStrToTokHeapBase);
        return true;
    } break;
    }
    return false;
}

EXPORT void CCONV destroyStructRepresentation(TOK structId, void *src, size_t srcSizeInBytes) {
    if (srcSizeInBytes < getSizeRequiredForStruct(structId)) {
        assert(false);
        return;
    }
    switch (structId) {
    default:
        return;
    case TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT:
        reinterpret_cast<GmmResourceInfoWinStruct *>(src)->~GmmResourceInfoWinStruct();
        break;
    }
}

uint64_t translatorVersion = 0;
EXPORT uint64_t CCONV getVersion() {
    return translatorVersion;
}
}
