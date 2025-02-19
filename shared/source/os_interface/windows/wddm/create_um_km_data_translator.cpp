/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/wsl_compute_helper/source/gmm_resource_info_accessor.h"
#include "shared/wsl_compute_helper/source/wsl_compute_helper.h"
#include "shared/wsl_compute_helper/source/wsl_compute_helper_types_demarshall.h"
#include "shared/wsl_compute_helper/source/wsl_compute_helper_types_marshall.h"
#include "shared/wsl_compute_helper/source/wsl_compute_helper_types_tokens.h"

#include <codecvt>
#include <locale>
#include <vector>

namespace NEO {

extern const char *wslComputeHelperLibNameToLoad;
const char *wslComputeHelperLibNameToLoad = wslComputeHelperLibName;

class WslComputeHelperUmKmDataTranslator;

class WslComputeHelperGmmHandleAllocator : public GmmHandleAllocator {
  public:
    WslComputeHelperGmmHandleAllocator(WslComputeHelperUmKmDataTranslator *translator);

    void *createHandle(const GMM_RESOURCE_INFO *gmmResourceInfo) override;

    bool openHandle(void *handle, GMM_RESOURCE_INFO *dstResInfo, size_t handleSize) override;

    void destroyHandle(void *handle) override;

    size_t getHandleSize() override {
        return handleSize;
    }

  protected:
    WslComputeHelperUmKmDataTranslator *translator = nullptr;
    size_t handleSize = 0U;
};

class WslComputeHelperUmKmDataTranslator : public UmKmDataTranslator {
  public:
    WslComputeHelperUmKmDataTranslator(std::unique_ptr<OsLibrary> wslComputeHelperLibrary)
        : wslComputeHelperLibrary(std::move(wslComputeHelperLibrary)) {
        UNRECOVERABLE_IF(nullptr == this->wslComputeHelperLibrary);

        auto procAddr = this->wslComputeHelperLibrary->getProcAddress(getSizeRequiredForStructName);
        UNRECOVERABLE_IF(nullptr == procAddr);
        auto getStructSizeFn = reinterpret_cast<getSizeRequiredForStructFPT>(procAddr);
        adapterInfoStructSize = getStructSizeFn(TOK_S_ADAPTER_INFO);
        createContextDataStructSize = getStructSizeFn(TOK_S_CREATECONTEXT_PVTDATA);
        commandBufferHeaderStructSize = getStructSizeFn(TOK_S_COMMAND_BUFFER_HEADER_REC);
        gmmResourceInfoStructSize = getStructSizeFn(TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT);
        gmmGfxPartitioningStructSize = getStructSizeFn(TOK_S_GMM_GFX_PARTITIONING);

        procAddr = this->wslComputeHelperLibrary->getProcAddress(getSizeRequiredForTokensName);
        UNRECOVERABLE_IF(nullptr == procAddr);
        auto getTokensSizeFn = reinterpret_cast<getSizeRequiredForTokensFPT>(procAddr);
        adapterInfoTokensSize = getTokensSizeFn(TOK_S_ADAPTER_INFO);
        gmmResourceInfoTokensSize = getTokensSizeFn(TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT);
        gmmGfxPartitioningTokensSize = getTokensSizeFn(TOK_S_GMM_GFX_PARTITIONING);

        procAddr = this->wslComputeHelperLibrary->getProcAddress(structToTokensName);
        UNRECOVERABLE_IF(nullptr == procAddr);
        structToTokensFn = reinterpret_cast<structToTokensFPT>(procAddr);

        procAddr = this->wslComputeHelperLibrary->getProcAddress(tokensToStructName);
        UNRECOVERABLE_IF(nullptr == procAddr);
        tokensToStructFn = reinterpret_cast<tokensToStructFPT>(procAddr);

        procAddr = this->wslComputeHelperLibrary->getProcAddress(destroyStructRepresentationName);
        UNRECOVERABLE_IF(nullptr == procAddr);
        destroyStructFn = reinterpret_cast<destroyStructRepresentationFPT>(procAddr);

        procAddr = this->wslComputeHelperLibrary->getProcAddress(getVersionName);
        UNRECOVERABLE_IF(nullptr == procAddr);
        computeHelperLibraryVersion = reinterpret_cast<getVersionFPT>(procAddr)();

        this->isEnabled = true;
    }

    ~WslComputeHelperUmKmDataTranslator() override = default;

    size_t getSizeForAdapterInfoInternalRepresentation() override {
        return adapterInfoStructSize;
    }

    size_t getSizeForCreateContextDataInternalRepresentation() override {
        return createContextDataStructSize;
    }

    size_t getSizeForCommandBufferHeaderDataInternalRepresentation() override {
        return commandBufferHeaderStructSize;
    }

    size_t getSizeForGmmResourceInfoInternalRepresentation() {
        return gmmResourceInfoStructSize;
    }

    size_t getSizeForGmmGfxPartitioningInternalRepresentation() override {
        if (computeHelperLibraryVersion == 0) {
            return sizeof(GMM_GFX_PARTITIONING);
        }

        return gmmGfxPartitioningStructSize;
    }

    bool translateAdapterInfoFromInternalRepresentation(ADAPTER_INFO_KMD &dst, const void *src, size_t srcSize) override {
        std::vector<uint8_t> tokData(adapterInfoTokensSize);
        TokenHeader *tok = reinterpret_cast<TokenHeader *>(tokData.data());
        if (false == structToTokensFn(TOK_S_ADAPTER_INFO, tok, adapterInfoTokensSize, src, srcSize)) {
            return false;
        }
        bool success = Demarshaller<TOK_S_ADAPTER_INFO>::demarshall(dst, tok, tok + adapterInfoTokensSize / sizeof(TokenHeader));
        if (IGFX_TIGERLAKE_LP != 33) {
            auto prod = static_cast<uint32_t>(dst.GfxPlatform.eProductFamily);
            switch (prod) {
            default:
                break;
            case 33:
                prod = IGFX_TIGERLAKE_LP;
                break;
            case 35:
                prod = IGFX_ROCKETLAKE;
                break;
            case 36:
                prod = IGFX_ALDERLAKE_S;
                break;
            case 37:
                prod = IGFX_ALDERLAKE_P;
                break;
            case 38:
                prod = IGFX_ALDERLAKE_N;
                break;
            }
            dst.GfxPlatform.eProductFamily = static_cast<PRODUCT_FAMILY>(prod);
        }
        propagateData(dst);
        return success;
    }

    bool translateCreateContextDataToInternalRepresentation(void *dst, size_t dstSize, const CREATECONTEXT_PVTDATA &src) override {
        auto marshalled = Marshaller<TOK_S_CREATECONTEXT_PVTDATA>::marshall(src);
        return tokensToStructFn(TOK_S_CREATECONTEXT_PVTDATA, dst, dstSize, &marshalled.base.header, reinterpret_cast<TokenHeader *>(&marshalled + 1));
    }

    bool translateCommandBufferHeaderDataToInternalRepresentation(void *dst, size_t dstSize, const COMMAND_BUFFER_HEADER &src) override {
        auto marshalled = Marshaller<TOK_S_COMMAND_BUFFER_HEADER_REC>::marshall(src);
        return tokensToStructFn(TOK_S_COMMAND_BUFFER_HEADER_REC, dst, dstSize, &marshalled.base.header, reinterpret_cast<TokenHeader *>(&marshalled + 1));
    }

    bool translateGmmResourceInfoToInternalRepresentation(void *dst, size_t dstSize, const GMM_RESOURCE_INFO &src) {
        GmmResourceInfoWinStruct resInfoPodStruct = {};
        static_cast<const GmmResourceInfoWinAccessor *>(&src)->get(resInfoPodStruct);
        auto marshalled = Marshaller<TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT>::marshall(resInfoPodStruct);
        return tokensToStructFn(TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT, dst, dstSize, &marshalled.base.header, reinterpret_cast<TokenHeader *>(&marshalled + 1));
    }

    bool translateGmmResourceInfoFromInternalRepresentation(GmmResourceInfoWinStruct &resInfoPodStruct, const void *src, size_t srcSize) {
        std::vector<uint8_t> tokData(gmmResourceInfoTokensSize);
        TokenHeader *tok = reinterpret_cast<TokenHeader *>(tokData.data());
        if (false == structToTokensFn(TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT, tok, gmmResourceInfoTokensSize, src, srcSize)) {
            return false;
        }
        return Demarshaller<TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT>::demarshall(resInfoPodStruct, tok, tok + gmmResourceInfoTokensSize / sizeof(TokenHeader));
    }

    bool translateGmmGfxPartitioningToInternalRepresentation(void *dst, size_t dstSize, const GMM_GFX_PARTITIONING &src) override {
        if (computeHelperLibraryVersion == 0) {
            return (0 == memcpy_s(dst, dstSize, &src, sizeof(src)));
        }

        auto marshalled = Marshaller<TOK_S_GMM_GFX_PARTITIONING>::marshall(src);
        return tokensToStructFn(TOK_S_GMM_GFX_PARTITIONING, dst, dstSize, &marshalled.base.header, reinterpret_cast<TokenHeader *>(&marshalled + 1));
    }

    bool translateGmmGfxPartitioningFromInternalRepresentation(GMM_GFX_PARTITIONING &dst, const void *src, size_t srcSize) override {
        if (computeHelperLibraryVersion == 0) {
            return (0 == memcpy_s(&dst, sizeof(GMM_GFX_PARTITIONING), src, srcSize));
        }

        std::vector<uint8_t> tokData(gmmGfxPartitioningTokensSize);
        TokenHeader *tok = reinterpret_cast<TokenHeader *>(tokData.data());
        if (false == structToTokensFn(TOK_S_GMM_GFX_PARTITIONING, tok, gmmGfxPartitioningTokensSize, src, srcSize)) {
            return false;
        }
        return Demarshaller<TOK_S_GMM_GFX_PARTITIONING>::demarshall(dst, tok, tok + gmmGfxPartitioningTokensSize / sizeof(TokenHeader));
    }

    void destroyGmmResourceInfo(void *src, size_t size) {
        destroyStructFn(TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT, src, size);
    }

    std::unique_ptr<GmmHandleAllocator> createGmmHandleAllocator() override {
        return std::make_unique<WslComputeHelperGmmHandleAllocator>(this);
    }

  protected:
    std::unique_ptr<OsLibrary> wslComputeHelperLibrary;
    uint64_t computeHelperLibraryVersion = 0U;

    structToTokensFPT structToTokensFn = nullptr;
    tokensToStructFPT tokensToStructFn = nullptr;
    destroyStructRepresentationFPT destroyStructFn = nullptr;

    size_t adapterInfoStructSize = 0U;
    size_t adapterInfoTokensSize = 0U;
    size_t createContextDataStructSize = 0U;
    size_t commandBufferHeaderStructSize = 0U;
    size_t gmmResourceInfoStructSize = 0U;
    size_t gmmResourceInfoTokensSize = 0U;
    size_t gmmGfxPartitioningStructSize = 0U;
    size_t gmmGfxPartitioningTokensSize = 0U;
};

WslComputeHelperGmmHandleAllocator::WslComputeHelperGmmHandleAllocator(WslComputeHelperUmKmDataTranslator *translator)
    : translator(translator) {
    UNRECOVERABLE_IF(nullptr == translator);
    this->handleSize = translator->getSizeForGmmResourceInfoInternalRepresentation();
}

void *WslComputeHelperGmmHandleAllocator::createHandle(const GMM_RESOURCE_INFO *gmmResourceInfo) {
    size_t sizeU64 = (translator->getSizeForGmmResourceInfoInternalRepresentation() + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    std::unique_ptr<uint64_t[]> ret{new uint64_t[sizeU64]};
    memset(ret.get(), 0, sizeU64 * sizeof(uint64_t));
    translator->translateGmmResourceInfoToInternalRepresentation(ret.get(), sizeU64 * sizeof(uint64_t), *gmmResourceInfo);

    return ret.release();
}

bool WslComputeHelperGmmHandleAllocator::openHandle(void *handle, GMM_RESOURCE_INFO *dstResInfo, size_t handleSize) {
    GmmResourceInfoWinStruct resInfoPodStruct = {};

    translator->translateGmmResourceInfoFromInternalRepresentation(resInfoPodStruct, handle, handleSize);

    static_cast<GmmResourceInfoWinAccessor *>(dstResInfo)->set(resInfoPodStruct);

    return true;
}

void WslComputeHelperGmmHandleAllocator::destroyHandle(void *handle) {
    translator->destroyGmmResourceInfo(handle, translator->getSizeForGmmResourceInfoInternalRepresentation());
    delete[] reinterpret_cast<uint64_t *>(handle);
}

std::unique_ptr<UmKmDataTranslator> createUmKmDataTranslator(const Gdi &gdi, D3DKMT_HANDLE adapter) {
    bool requiresWslComputeHelper = false;
#if !defined(_WIN32)
    requiresWslComputeHelper = true;
#endif
    if (requiresWslComputeHelper || NEO::debugManager.flags.UseUmKmDataTranslator.get()) {
        auto wpath = queryAdapterDriverStorePath(gdi, adapter);
        std::string path;
        if (strlen(wslComputeHelperLibNameToLoad)) {
            path.reserve(wpath.size() + 1 + strlen(wslComputeHelperLibName));
            for (wchar_t wc : wpath) {
                path += static_cast<char>(wc);
            }
            path.append(Os::fileSeparator);
            path.append(wslComputeHelperLibNameToLoad);
        }
        std::unique_ptr<OsLibrary> lib{OsLibrary::loadFunc(path)};
        if ((nullptr != lib) && (lib->isLoaded())) {
            return std::make_unique<WslComputeHelperUmKmDataTranslator>(std::move(lib));
        }
    }
    return std::make_unique<UmKmDataTranslator>();
}

} // namespace NEO
