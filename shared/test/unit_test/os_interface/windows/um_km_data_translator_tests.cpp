/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"

#include "test.h"

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
    ret = translator.tranlateCommandBufferHeaderDataToInternalRepresentation(&dstCmdBufferHeader, sizeof(COMMAND_BUFFER_HEADER), srcCmdBufferHeader);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0, memcmp(&dstCmdBufferHeader, &srcCmdBufferHeader, sizeof(COMMAND_BUFFER_HEADER)));

    CREATECONTEXT_PVTDATA srcCreateContextPvtData, dstCreateContextPvtData;
    memset(&srcCreateContextPvtData, 7, sizeof(srcCreateContextPvtData));
    ret = translator.translateCreateContextDataToInternalRepresentation(&dstCreateContextPvtData, sizeof(CREATECONTEXT_PVTDATA), srcCreateContextPvtData);
    EXPECT_TRUE(ret);
    EXPECT_EQ(0, memcmp(&dstCreateContextPvtData, &srcCreateContextPvtData, sizeof(CREATECONTEXT_PVTDATA)));
}
