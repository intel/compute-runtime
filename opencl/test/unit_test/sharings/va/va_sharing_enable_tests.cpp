/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/source/sharings/va/enable_va.h"
#include "opencl/source/sharings/va/va_sharing_functions.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

static int vaDisplayIsValidRet = 1;
extern "C" int vaDisplayIsValid(VADisplay vaDisplay) {
    return vaDisplayIsValidRet;
}

class MockDriverInfo : public DriverInfo {
  public:
    MockDriverInfo(bool imageSupport) : imageSupport(imageSupport) {}
    bool getImageSupport() override { return imageSupport; };
    bool imageSupport = true;
};

class VaSharingEnablerTests : public MemoryManagementFixture,
                              public ::testing::Test {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        factory.reset(new VaSharingBuilderFactory());
        ASSERT_NE(nullptr, factory.get());
    }

    void TearDown() override {
        factory.reset(nullptr);
        MemoryManagementFixture::TearDown();
    }
    std::unique_ptr<VaSharingBuilderFactory> factory;
};

TEST_F(VaSharingEnablerTests, givenVaFactoryWhenImagesUnsupportedOrLibVaUnavailableThenNoExtensionIsReturned) {
    // hijack dlopen function
    VariableBackup<std::function<void *(const char *, int)>> bkp(&VASharingFunctions::fdlopen);
    bkp = [&](const char *filename, int flag) -> void * {
        // no libva in system
        return nullptr;
    };
    for (bool imagesSupported : {false, true}) {
        MockDriverInfo driverInfo(imagesSupported);
        auto ext = factory->getExtensions(&driverInfo);
        EXPECT_EQ(0u, ext.length());
        EXPECT_STREQ("", ext.c_str());
    }
}

TEST_F(VaSharingEnablerTests, givenVaFactoryWhenImagesSupportedAndLibVaAvailableThenExtensionStringIsReturned) {
    VariableBackup<std::function<void *(const char *, int)>> bkpOpen(&VASharingFunctions::fdlopen);
    bkpOpen = [&](const char *filename, int flag) -> void * {
        return this;
    };
    VariableBackup<std::function<int(void *)>> bkpClose(&VASharingFunctions::fdlclose);
    bkpClose = [&](void *handle) -> int {
        return 0;
    };
    VariableBackup<std::function<void *(void *, const char *)>> bkpSym(&VASharingFunctions::fdlsym);
    bkpSym = [&](void *handle, const char *symbol) -> void * {
        return nullptr;
    };
    for (bool imagesSupported : {false, true}) {
        MockDriverInfo driverInfo(imagesSupported);
        auto ext = factory->getExtensions(&driverInfo);
        EXPECT_STREQ(imagesSupported ? "cl_intel_va_api_media_sharing " : "", ext.c_str());
    }
}

TEST_F(VaSharingEnablerTests, givenVaFactoryWhenAskedThenGlobalIcdIsConfigured) {
    class CrtRestore {
      public:
        CrtRestore() {
            crtSnapshot = crtGlobalDispatchTable;
        }
        ~CrtRestore() {
            crtGlobalDispatchTable = crtSnapshot;
        }
        decltype(crtGlobalDispatchTable) crtSnapshot;
    };

    // we play with global table, so first save state then restore it with use of RAII
    CrtRestore crtRestore;
    crtGlobalDispatchTable.clCreateFromVA_APIMediaSurfaceINTEL = nullptr;
    crtGlobalDispatchTable.clEnqueueReleaseVA_APIMediaSurfacesINTEL = nullptr;
    crtGlobalDispatchTable.clEnqueueAcquireVA_APIMediaSurfacesINTEL = nullptr;
    crtGlobalDispatchTable.clGetDeviceIDsFromVA_APIMediaAdapterINTEL = nullptr;
    factory->fillGlobalDispatchTable();
    EXPECT_NE(nullptr, crtGlobalDispatchTable.clCreateFromVA_APIMediaSurfaceINTEL);
    EXPECT_NE(nullptr, crtGlobalDispatchTable.clEnqueueAcquireVA_APIMediaSurfacesINTEL);
    EXPECT_NE(nullptr, crtGlobalDispatchTable.clEnqueueAcquireVA_APIMediaSurfacesINTEL);
    EXPECT_NE(nullptr, crtGlobalDispatchTable.clGetDeviceIDsFromVA_APIMediaAdapterINTEL);
}

TEST_F(VaSharingEnablerTests, givenVaFactoryWhenAskedThenBuilderIsCreated) {
    auto builder = factory->createContextBuilder();
    EXPECT_NE(nullptr, builder);
}

TEST_F(VaSharingEnablerTests, givenVaBuilderWhenUnknownPropertyThenFalseIsReturned) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_CONTEXT_PLATFORM;
    cl_context_properties value;
    auto res = builder->processProperties(property, value);
    EXPECT_FALSE(res);
}

TEST_F(VaSharingEnablerTests, givenVaBuilderWhenValidPropertyThenTrueIsReturned) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_CONTEXT_VA_API_DISPLAY_INTEL;
    cl_context_properties value = 0x1243;
    auto res = builder->processProperties(property, value);
    EXPECT_TRUE(res);

    //repeat to check if we don't allocate twice
    auto prevAllocations = MemoryManagement::numAllocations.load();
    res = builder->processProperties(property, value);
    EXPECT_TRUE(res);
    auto currAllocations = MemoryManagement::numAllocations.load();
    EXPECT_EQ(prevAllocations, currAllocations);
}

TEST_F(VaSharingEnablerTests, givenVaBuilderWhenNoPropertiesThenFinalizerReturnsTrue) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    auto res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_TRUE(res);
    EXPECT_EQ(CL_SUCCESS, errcodeRet);
}

TEST_F(VaSharingEnablerTests, givenVaBuilderWhenInvalidPropertiesThenFinalizerReturnsTrue) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_CONTEXT_PLATFORM;
    cl_context_properties value;
    auto res = builder->processProperties(property, value);
    EXPECT_FALSE(res);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_TRUE(res);
    EXPECT_EQ(CL_SUCCESS, errcodeRet);
}

TEST_F(VaSharingEnablerTests, givenVaBuilderWhenValidPropertyButInvalidDisplayThenFinalizerReturnsFalseAndErrcode) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    vaDisplayIsValidRet = 0;
    cl_context_properties property = CL_CONTEXT_VA_API_DISPLAY_INTEL;
    cl_context_properties value = 0x10000;
    auto res = builder->processProperties(property, value);
    EXPECT_TRUE(res);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_FALSE(res);
    EXPECT_EQ(CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL, errcodeRet);
}

TEST_F(VaSharingEnablerTests, givenVaBuilderWhenValidPropertyButValidDisplayThenFinalizerReturnsTrue) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    vaDisplayIsValidRet = 1;
    cl_context_properties property = CL_CONTEXT_VA_API_DISPLAY_INTEL;
    cl_context_properties value = 0x10000;
    auto res = builder->processProperties(property, value);
    EXPECT_TRUE(res);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_TRUE(res);
    EXPECT_EQ(CL_SUCCESS, errcodeRet);
}
