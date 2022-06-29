/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/windows/adapter_info_tests.h"
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

constexpr char *mockTokToStrAdapterString = "MOCK_ADAPTER_TOK_TO_STR";
constexpr uint16_t mockTokToStrDriverBuildNumber = 0xabcd;
constexpr uint32_t mockTokToStrProcessID = 0xabcdefbc;
constexpr uint64_t mockTokToStrHeapBase = 0xabcdefbc;

constexpr char *mockStrToTokAdapterString = "MOCK_ADAPTER_STR_TO_TOK";
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
        return (0 == memcpy_s(adapterInfo->AdapterString, sizeof(adapterInfo->AdapterString), mockTokToStrAdapterString, strlen(mockTokToStrAdapterString) + 1));
    } break;
    case TOK_S_COMMAND_BUFFER_HEADER_REC: {
        auto cmdBufferHeader = new (dst) COMMAND_BUFFER_HEADER{};
        cmdBufferHeader->DriverBuildNumber = mockTokToStrDriverBuildNumber;
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

TEST(UmKmDataTranslator, whenCreatingDefaultTranslatorThenTranslationIsDisabled) {
    NEO::Gdi gdi;
    auto translator = NEO::createUmKmDataTranslator(gdi, 0);
    EXPECT_FALSE(translator->enabled());
}

TEST(UmKmDataTranslator, whenUsingDefaultTranslatorThenTranslationIsDisabled) {
    NEO::UmKmDataTranslator translator;
    EXPECT_FALSE(translator.enabled());
    EXPECT_FALSE(translator.createGmmHandleAllocator());
    EXPECT_EQ(sizeof(ADAPTER_INFO), translator.getSizeForAdapterInfoInternalRepresentation());
    EXPECT_EQ(sizeof(COMMAND_BUFFER_HEADER), translator.getSizeForCommandBufferHeaderDataInternalRepresentation());
    EXPECT_EQ(sizeof(CREATECONTEXT_PVTDATA), translator.getSizeForCreateContextDataInternalRepresentation());

    ADAPTER_INFO srcAdapterInfo, dstAdapterInfo;
    memset(&srcAdapterInfo, 7, sizeof(srcAdapterInfo));
    auto ret = translator.translateAdapterInfoFromInternalRepresentation(dstAdapterInfo, &srcAdapterInfo, sizeof(ADAPTER_INFO));
    EXPECT_TRUE(ret);
    EXPECT_EQ(0, memcmp(&dstAdapterInfo, &srcAdapterInfo, sizeof(ADAPTER_INFO)));

    COMMAND_BUFFER_HEADER srcCmdBufferHeader, dstCmdBufferHeader;
    memset(&srcCmdBufferHeader, 7, sizeof(srcCmdBufferHeader));
    ret = translator.translateCommandBufferHeaderDataToInternalRepresentation(&dstCmdBufferHeader, sizeof(COMMAND_BUFFER_HEADER), srcCmdBufferHeader);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0, memcmp(&dstCmdBufferHeader, &srcCmdBufferHeader, sizeof(COMMAND_BUFFER_HEADER)));

    CREATECONTEXT_PVTDATA srcCreateContextPvtData, dstCreateContextPvtData;
    memset(&srcCreateContextPvtData, 7, sizeof(srcCreateContextPvtData));
    ret = translator.translateCreateContextDataToInternalRepresentation(&dstCreateContextPvtData, sizeof(CREATECONTEXT_PVTDATA), srcCreateContextPvtData);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0, memcmp(&dstCreateContextPvtData, &srcCreateContextPvtData, sizeof(CREATECONTEXT_PVTDATA)));
}

TEST(UmKmDataTranslator, givenToggledDebugKeyWhenCreatingDefaultTranslatorThenTranslationIsEnabled) {
    DebugManagerStateRestore debugSettingsRestore;

    NEO::DebugManager.flags.UseUmKmDataTranslator.set(true);
    NEO::wslComputeHelperLibNameToLoad = "";
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    auto translator = NEO::createUmKmDataTranslator(gdi, handle);
    EXPECT_TRUE(translator->enabled());
}

TEST(WslUmKmDataTranslator, whenQueryingForTranslationThenQueryIsForwardedToWslComputeHelper) {
    DebugManagerStateRestore debugSettingsRestore;

    NEO::DebugManager.flags.UseUmKmDataTranslator.set(true);
    NEO::wslComputeHelperLibNameToLoad = "";
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    auto translator = NEO::createUmKmDataTranslator(gdi, handle);

    EXPECT_EQ(sizeof(ADAPTER_INFO), translator->getSizeForAdapterInfoInternalRepresentation());
    EXPECT_EQ(sizeof(COMMAND_BUFFER_HEADER), translator->getSizeForCommandBufferHeaderDataInternalRepresentation());
    EXPECT_EQ(sizeof(CREATECONTEXT_PVTDATA), translator->getSizeForCreateContextDataInternalRepresentation());

    ADAPTER_INFO srcAdapterInfo, dstAdapterInfo;
    memset(&srcAdapterInfo, 7, sizeof(srcAdapterInfo));
    auto ret = translator->translateAdapterInfoFromInternalRepresentation(dstAdapterInfo, &srcAdapterInfo, sizeof(ADAPTER_INFO));
    EXPECT_TRUE(ret);
    EXPECT_EQ(0, memcmp(dstAdapterInfo.DeviceRegistryPath, mockStrToTokAdapterString, strlen(mockStrToTokAdapterString) + 1));

    COMMAND_BUFFER_HEADER srcCmdBufferHeader, dstCmdBufferHeader;
    memset(&srcCmdBufferHeader, 7, sizeof(srcCmdBufferHeader));
    ret = translator->translateCommandBufferHeaderDataToInternalRepresentation(&dstCmdBufferHeader, sizeof(COMMAND_BUFFER_HEADER), srcCmdBufferHeader);
    EXPECT_TRUE(ret);
    EXPECT_EQ(mockTokToStrDriverBuildNumber, dstCmdBufferHeader.DriverBuildNumber);

    CREATECONTEXT_PVTDATA srcCreateContextPvtData, dstCreateContextPvtData;
    memset(&srcCreateContextPvtData, 7, sizeof(srcCreateContextPvtData));
    ret = translator->translateCreateContextDataToInternalRepresentation(&dstCreateContextPvtData, sizeof(CREATECONTEXT_PVTDATA), srcCreateContextPvtData);
    EXPECT_TRUE(ret);
    EXPECT_EQ(mockTokToStrProcessID, dstCreateContextPvtData.ProcessID);

    auto gmmHandleAllocator = translator->createGmmHandleAllocator();
    ASSERT_NE(nullptr, gmmHandleAllocator);
    gmmHandleAllocator->getHandleSize();
    UmKmDataTempStorage<GMM_RESOURCE_INFO> gmmResInfo;
    EXPECT_EQ(sizeof(GmmResourceInfoWinStruct), gmmHandleAllocator->getHandleSize());
    auto gmmResourceInfoHandle = gmmHandleAllocator->createHandle(reinterpret_cast<GMM_RESOURCE_INFO *>(gmmResInfo.data()));
    ASSERT_NE(nullptr, gmmResourceInfoHandle);
    EXPECT_EQ(mockTokToStrDriverBuildNumber, reinterpret_cast<GmmResourceInfoWinStruct *>(gmmResourceInfoHandle)->GmmResourceInfoCommon.pPrivateData);
    gmmHandleAllocator->destroyHandle(gmmResourceInfoHandle);
}

TEST(WslUmKmDataTranslator, whenOpeningExistingHandleThenResourceInfoIsCopiedBasedOnTranslationResult) {
    DebugManagerStateRestore debugSettingsRestore;

    NEO::DebugManager.flags.UseUmKmDataTranslator.set(true);
    NEO::wslComputeHelperLibNameToLoad = "";
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    auto translator = NEO::createUmKmDataTranslator(gdi, handle);
    auto gmmHandleAllocator = translator->createGmmHandleAllocator();
    const auto handleSize = gmmHandleAllocator->getHandleSize();

    UmKmDataTempStorage<GMM_RESOURCE_INFO> gmmResInfo(handleSize);
    auto gmmResourceInfoHandle = gmmHandleAllocator->createHandle(reinterpret_cast<GMM_RESOURCE_INFO *>(gmmResInfo.data()));
    EXPECT_EQ(mockTokToStrDriverBuildNumber, reinterpret_cast<GmmResourceInfoWinStruct *>(gmmResourceInfoHandle)->GmmResourceInfoCommon.pPrivateData);

    UmKmDataTempStorage<GMM_RESOURCE_INFO> gmmResInfoDst(handleSize);
    gmmHandleAllocator->openHandle(gmmResourceInfoHandle, reinterpret_cast<GMM_RESOURCE_INFO *>(gmmResInfoDst.data()), handleSize);
    EXPECT_EQ(mockStrToTokDriverBuildNumber, reinterpret_cast<uint64_t>(reinterpret_cast<GMM_RESOURCE_INFO *>(gmmResInfoDst.data())->GetPrivateData()));

    UmKmDataTempStorage<GMM_RESOURCE_INFO> gmmResInfoDst2(handleSize);
    gmmHandleAllocator->openHandle(gmmResourceInfoHandle, reinterpret_cast<GMM_RESOURCE_INFO *>(gmmResInfoDst2.data()), sizeof(TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT) + 4);
    EXPECT_EQ(0u, reinterpret_cast<uint64_t>(reinterpret_cast<GMM_RESOURCE_INFO *>(gmmResInfoDst2.data())->GetPrivateData()));

    gmmHandleAllocator->destroyHandle(gmmResourceInfoHandle);
}

TEST(WslUmKmDataTranslator, whenTranslatingGraphicsPartitionThenResultIsBasedOnWslComputeHelperVersion) {
    DebugManagerStateRestore debugSettingsRestore;

    NEO::DebugManager.flags.UseUmKmDataTranslator.set(true);
    NEO::wslComputeHelperLibNameToLoad = "";
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    ASSERT_EQ(translatorVersion, 0U);
    auto translatorV0 = NEO::createUmKmDataTranslator(gdi, handle);
    translatorVersion = 1;
    auto translatorV1 = NEO::createUmKmDataTranslator(gdi, handle);
    translatorVersion = 0;

    EXPECT_EQ(sizeof(GMM_GFX_PARTITIONING), translatorV0->getSizeForGmmGfxPartitioningInternalRepresentation());
    EXPECT_EQ(sizeof(GMM_GFX_PARTITIONING) + 4, translatorV1->getSizeForGmmGfxPartitioningInternalRepresentation());

    GMM_GFX_PARTITIONING src = {}, dst = {};
    src.Heap32->Base = 7;
    dst.Heap32->Base = 0;
    auto ret = translatorV0->translateGmmGfxPartitioningToInternalRepresentation(&dst, sizeof(GMM_GFX_PARTITIONING), src);
    EXPECT_TRUE(ret);
    EXPECT_EQ(7u, dst.Heap32->Base);

    src.Heap32->Base = 7;
    dst.Heap32->Base = 0;
    ret = translatorV1->translateGmmGfxPartitioningToInternalRepresentation(&dst, sizeof(GMM_GFX_PARTITIONING), src);
    EXPECT_FALSE(ret);
    EXPECT_EQ(0u, dst.Heap32->Base);

    src.Heap32->Base = 7;
    dst.Heap32->Base = 0;
    ret = translatorV1->translateGmmGfxPartitioningToInternalRepresentation(&dst, sizeof(GMM_GFX_PARTITIONING) + 4, src);
    EXPECT_TRUE(ret);
    EXPECT_EQ(mockTokToStrHeapBase, dst.Heap32->Base);

    src.Heap32->Base = 7;
    dst.Heap32->Base = 0;
    ret = translatorV0->translateGmmGfxPartitioningFromInternalRepresentation(dst, &src, sizeof(GMM_GFX_PARTITIONING));
    EXPECT_TRUE(ret);
    EXPECT_EQ(7u, dst.Heap32->Base);

    src.Heap32->Base = 7;
    dst.Heap32->Base = 0;
    ret = translatorV1->translateGmmGfxPartitioningFromInternalRepresentation(dst, &src, sizeof(GMM_GFX_PARTITIONING));
    EXPECT_FALSE(ret);
    EXPECT_EQ(0u, dst.Heap32->Base);

    src.Heap32->Base = 7;
    dst.Heap32->Base = 0;
    ret = translatorV1->translateGmmGfxPartitioningFromInternalRepresentation(dst, &src, sizeof(GMM_GFX_PARTITIONING) + 4);
    EXPECT_TRUE(ret);
    EXPECT_EQ(mockStrToTokHeapBase, dst.Heap32->Base);
}
