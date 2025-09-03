/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/rtas/rtas.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

struct RTASFixtureExt : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        builderCreateCalled = 0;
        builderCreateCalled = 0;
        builderCreateFailCalled = 0;
        builderDestroyCalled = 0;
        builderDestroyFailCalled = 0;
        builderGetBuildPropertiesCalled = 0;
        builderGetBuildPropertiesFailCalled = 0;
        builderBuildCalled = 0;
        builderBuildFailCalled = 0;
        formatCompatibilityCheckCalled = 0;
        formatCompatibilityCheckFailCalled = 0;
        parallelOperationDestroyCalled = 0;
        parallelOperationDestroyFailCalled = 0;
        parallelOperationCreateCalled = 0;
        parallelOperationCreateFailCalled = 0;
        parallelOperationGetPropertiesCalled = 0;
        parallelOperationGetPropertiesFailCalled = 0;
        parallelOperationJoinCalled = 0;
        parallelOperationJoinFailCalled = 0;
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    static ze_result_t builderCreate(ze_driver_handle_t hDriver,
                                     const ze_rtas_builder_ext_desc_t *pDescriptor,
                                     ze_rtas_builder_ext_handle_t *phBuilder) {
        builderCreateCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderCreateFail(ze_driver_handle_t hDriver,
                                         const ze_rtas_builder_ext_desc_t *pDescriptor,
                                         ze_rtas_builder_ext_handle_t *phBuilder) {
        builderCreateFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderDestroy(ze_rtas_builder_ext_handle_t hBuilder) {
        builderDestroyCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderDestroyFail(ze_rtas_builder_ext_handle_t hBuilder) {
        builderDestroyFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderGetBuildProperties(ze_rtas_builder_ext_handle_t hBuilder,
                                                 const ze_rtas_builder_build_op_ext_desc_t *args,
                                                 ze_rtas_builder_ext_properties_t *pProp) {
        builderGetBuildPropertiesCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderGetBuildPropertiesFail(ze_rtas_builder_ext_handle_t hBuilder,
                                                     const ze_rtas_builder_build_op_ext_desc_t *args,
                                                     ze_rtas_builder_ext_properties_t *pProp) {
        builderGetBuildPropertiesFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderBuild(ze_rtas_builder_ext_handle_t hBuilder,
                                    const ze_rtas_builder_build_op_ext_desc_t *args,
                                    void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                    void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                    ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
                                    void *pBuildUserPtr, ze_rtas_aabb_ext_t *pBounds,
                                    size_t *pRtasBufferSizeBytes) {
        builderBuildCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderBuildFail(ze_rtas_builder_ext_handle_t hBuilder,
                                        const ze_rtas_builder_build_op_ext_desc_t *args,
                                        void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                        void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                        ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
                                        void *pBuildUserPtr, ze_rtas_aabb_ext_t *pBounds,
                                        size_t *pRtasBufferSizeBytes) {
        builderBuildFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t formatCompatibilityCheck(ze_driver_handle_t hDriver,
                                                const ze_rtas_format_ext_t accelFormat,
                                                const ze_rtas_format_ext_t otherAccelFormat) {
        formatCompatibilityCheckCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t formatCompatibilityCheckFail(ze_driver_handle_t hDriver,
                                                    const ze_rtas_format_ext_t accelFormat,
                                                    const ze_rtas_format_ext_t otherAccelFormat) {
        formatCompatibilityCheckFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationDestroy(ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
        parallelOperationDestroyCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationDestroyFail(ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
        parallelOperationDestroyFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationCreate(ze_driver_handle_t hDriver,
                                               ze_rtas_parallel_operation_ext_handle_t *phParallelOperation) {
        parallelOperationCreateCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationCreateFail(ze_driver_handle_t hDriver,
                                                   ze_rtas_parallel_operation_ext_handle_t *phParallelOperation) {
        parallelOperationCreateFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationGetProperties(ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
                                                      ze_rtas_parallel_operation_ext_properties_t *pProperties) {
        parallelOperationGetPropertiesCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationGetPropertiesFail(ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
                                                          ze_rtas_parallel_operation_ext_properties_t *pProperties) {
        parallelOperationGetPropertiesFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationJoin(ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
        parallelOperationJoinCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationJoinFail(ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
        parallelOperationJoinFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static uint32_t builderCreateCalled;
    static uint32_t builderCreateFailCalled;
    static uint32_t builderDestroyCalled;
    static uint32_t builderDestroyFailCalled;
    static uint32_t builderGetBuildPropertiesCalled;
    static uint32_t builderGetBuildPropertiesFailCalled;
    static uint32_t builderBuildCalled;
    static uint32_t builderBuildFailCalled;
    static uint32_t formatCompatibilityCheckCalled;
    static uint32_t formatCompatibilityCheckFailCalled;
    static uint32_t parallelOperationDestroyCalled;
    static uint32_t parallelOperationDestroyFailCalled;
    static uint32_t parallelOperationCreateCalled;
    static uint32_t parallelOperationCreateFailCalled;
    static uint32_t parallelOperationGetPropertiesCalled;
    static uint32_t parallelOperationGetPropertiesFailCalled;
    static uint32_t parallelOperationJoinCalled;
    static uint32_t parallelOperationJoinFailCalled;
};

uint32_t RTASFixtureExt::builderCreateCalled = 0;
uint32_t RTASFixtureExt::builderCreateFailCalled = 0;
uint32_t RTASFixtureExt::builderDestroyCalled = 0;
uint32_t RTASFixtureExt::builderDestroyFailCalled = 0;
uint32_t RTASFixtureExt::builderGetBuildPropertiesCalled = 0;
uint32_t RTASFixtureExt::builderGetBuildPropertiesFailCalled = 0;
uint32_t RTASFixtureExt::builderBuildCalled = 0;
uint32_t RTASFixtureExt::builderBuildFailCalled = 0;
uint32_t RTASFixtureExt::formatCompatibilityCheckCalled = 0;
uint32_t RTASFixtureExt::formatCompatibilityCheckFailCalled = 0;
uint32_t RTASFixtureExt::parallelOperationDestroyCalled = 0;
uint32_t RTASFixtureExt::parallelOperationDestroyFailCalled = 0;
uint32_t RTASFixtureExt::parallelOperationCreateCalled = 0;
uint32_t RTASFixtureExt::parallelOperationCreateFailCalled = 0;
uint32_t RTASFixtureExt::parallelOperationGetPropertiesCalled = 0;
uint32_t RTASFixtureExt::parallelOperationGetPropertiesFailCalled = 0;
uint32_t RTASFixtureExt::parallelOperationJoinCalled = 0;
uint32_t RTASFixtureExt::parallelOperationJoinFailCalled = 0;

using RTASTestExt = Test<RTASFixtureExt>;

struct MockRTASExtOsLibrary : public OsLibrary {
  public:
    static bool mockLoad;
    MockRTASExtOsLibrary(const std::string &name, std::string *errorValue) {
    }
    MockRTASExtOsLibrary() {}
    ~MockRTASExtOsLibrary() override = default;
    void *getProcAddress(const std::string &procName) override {
        auto it = funcMap.find(procName);
        if (funcMap.end() == it) {
            return nullptr;
        } else {
            return it->second;
        }
    }
    bool isLoaded() override {
        return false;
    }
    std::string getFullPath() override {
        return std::string();
    }
    static OsLibrary *load(const OsLibraryCreateProperties &properties) {
        if (mockLoad == true) {
            auto ptr = new (std::nothrow) MockRTASExtOsLibrary();
            return ptr;
        } else {
            return nullptr;
        }
    }
    std::map<std::string, void *> funcMap;
};

bool MockRTASExtOsLibrary::mockLoad = true;

TEST_F(RTASTestExt, GivenLibraryLoadsSymbolsAndUnderlyingFunctionsSucceedThenSuccessIsReturned_Ext) {
    struct MockSymbolsLoadedOsLibrary : public OsLibrary {
      public:
        MockSymbolsLoadedOsLibrary(const std::string &name, std::string *errorValue) {}
        MockSymbolsLoadedOsLibrary() {}
        ~MockSymbolsLoadedOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            funcMap["zeRTASBuilderCreateExtImpl"] = reinterpret_cast<void *>(&builderCreate);
            funcMap["zeRTASBuilderDestroyExtImpl"] = reinterpret_cast<void *>(&builderDestroy);
            funcMap["zeRTASBuilderGetBuildPropertiesExtImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);
            funcMap["zeRTASBuilderBuildExtImpl"] = reinterpret_cast<void *>(&builderBuild);
            funcMap["zeDriverRTASFormatCompatibilityCheckExtImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);
            funcMap["zeRTASParallelOperationCreateExtImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);
            funcMap["zeRTASParallelOperationDestroyExtImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);
            funcMap["zeRTASParallelOperationGetPropertiesExtImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);
            funcMap["zeRTASParallelOperationJoinExtImpl"] = reinterpret_cast<void *>(&parallelOperationJoin);
            auto it = funcMap.find(procName);
            if (funcMap.end() == it) {
                return nullptr;
            } else {
                return it->second;
            }
        }
        bool isLoaded() override { return true; }
        std::string getFullPath() override { return std::string(); }
        static OsLibrary *load(const OsLibraryCreateProperties &properties) {
            auto ptr = new (std::nothrow) MockSymbolsLoadedOsLibrary();
            return ptr;
        }
        std::map<std::string, void *> funcMap;
    };
    ze_rtas_builder_ext_handle_t hBuilder;
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation;
    const ze_rtas_format_ext_t accelFormatA = {};
    const ze_rtas_format_ext_t accelFormatB = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockSymbolsLoadedOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExt(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(1u, builderCreateCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExt(hBuilder));
    EXPECT_EQ(1u, builderDestroyCalled);
    driverHandle->rtasLibraryHandle.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExt(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(1u, parallelOperationCreateCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExt(hParallelOperation));
    EXPECT_EQ(1u, parallelOperationDestroyCalled);
    driverHandle->rtasLibraryHandle.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExt(driverHandle->toHandle(), accelFormatA, accelFormatB));
    EXPECT_EQ(1u, formatCompatibilityCheckCalled);
    driverHandle->rtasLibraryHandle.reset();
}

TEST_F(RTASTestExt, GivenLibraryFailedToLoadSymbolsThenErrorIsReturned_Ext) {
    ze_rtas_builder_ext_handle_t hBuilder;
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation;
    const ze_rtas_format_ext_t accelFormatA = {};
    const ze_rtas_format_ext_t accelFormatB = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockRTASExtOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeRTASBuilderCreateExt(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeRTASParallelOperationCreateExt(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeDriverRTASFormatCompatibilityCheckExt(driverHandle->toHandle(), accelFormatA, accelFormatB));
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderCommandListAppendCopySucceedsThenSuccessIsReturned_Ext) {
    // Mock implementation for zeRTASBuilderCommandListAppendCopyExt
    static uint32_t builderCommandListAppendCopyCalled = 0;
    auto builderCommandListAppendCopy = [](ze_command_list_handle_t, ze_rtas_builder_ext_handle_t, const void *, size_t, void *, size_t, ze_rtas_parallel_operation_ext_handle_t, void *, ze_rtas_aabb_ext_t *, size_t *) -> ze_result_t {
        builderCommandListAppendCopyCalled++;
        return ZE_RESULT_SUCCESS;
    };

    // Simulate symbol loading
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    MockRTASExtOsLibrary *osLibHandle = static_cast<MockRTASExtOsLibrary *>(driverHandle->rtasLibraryHandle.get());
    osLibHandle->funcMap["zeRTASBuilderCommandListAppendCopyExtImpl"] = reinterpret_cast<void *>(+builderCommandListAppendCopy);

    // Patch the entry point loader to pick up our symbol
    L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get());

    // Call the function under test
    ze_command_list_handle_t hCommandList = reinterpret_cast<ze_command_list_handle_t>(0x1234);
    ze_rtas_builder_ext_handle_t hBuilder = reinterpret_cast<ze_rtas_builder_ext_handle_t>(0x5678);
    void *pScratchBuffer = nullptr;
    size_t scratchBufferSizeBytes = 0;
    void *pRtasBuffer = nullptr;
    size_t rtasBufferSizeBytes = 0;
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation = reinterpret_cast<ze_rtas_parallel_operation_ext_handle_t>(0x9abc);
    void *pBuildUserPtr = nullptr;
    ze_rtas_aabb_ext_t *pBounds = nullptr;
    size_t *pRtasBufferSizeBytes = nullptr;

    // If the function pointer is available, call it
    auto func = reinterpret_cast<ze_result_t (*)(ze_command_list_handle_t, ze_rtas_builder_ext_handle_t, const void *, size_t, void *, size_t, ze_rtas_parallel_operation_ext_handle_t, void *, ze_rtas_aabb_ext_t *, size_t *)>(
        osLibHandle->funcMap["zeRTASBuilderCommandListAppendCopyExtImpl"]);
    ASSERT_NE(nullptr, func);
    EXPECT_EQ(ZE_RESULT_SUCCESS, func(hCommandList, hBuilder, pScratchBuffer, scratchBufferSizeBytes, pRtasBuffer, rtasBufferSizeBytes, hParallelOperation, pBuildUserPtr, pBounds, pRtasBufferSizeBytes));
    EXPECT_NE(0u, builderCommandListAppendCopyCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderCommandListAppendCopyFailsThenErrorIsReturned_Ext) {
    // Mock implementation for zeRTASBuilderCommandListAppendCopyExt that fails
    static uint32_t builderCommandListAppendCopyFailCalled = 0;
    auto builderCommandListAppendCopyFail = [](ze_command_list_handle_t, ze_rtas_builder_ext_handle_t, const void *, size_t, void *, size_t, ze_rtas_parallel_operation_ext_handle_t, void *, ze_rtas_aabb_ext_t *, size_t *) -> ze_result_t {
        builderCommandListAppendCopyFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    };

    // Simulate symbol loading
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    MockRTASExtOsLibrary *osLibHandle = static_cast<MockRTASExtOsLibrary *>(driverHandle->rtasLibraryHandle.get());
    osLibHandle->funcMap["zeRTASBuilderCommandListAppendCopyExtImpl"] = reinterpret_cast<void *>(+builderCommandListAppendCopyFail);

    // Patch the entry point loader to pick up our symbol
    L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get());

    // Call the function under test
    ze_command_list_handle_t hCommandList = reinterpret_cast<ze_command_list_handle_t>(0x1234);
    ze_rtas_builder_ext_handle_t hBuilder = reinterpret_cast<ze_rtas_builder_ext_handle_t>(0x5678);
    void *pScratchBuffer = nullptr;
    size_t scratchBufferSizeBytes = 0;
    void *pRtasBuffer = nullptr;
    size_t rtasBufferSizeBytes = 0;
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation = reinterpret_cast<ze_rtas_parallel_operation_ext_handle_t>(0x9abc);
    void *pBuildUserPtr = nullptr;
    ze_rtas_aabb_ext_t *pBounds = nullptr;
    size_t *pRtasBufferSizeBytes = nullptr;

    // If the function pointer is available, call it
    auto func = reinterpret_cast<ze_result_t (*)(ze_command_list_handle_t, ze_rtas_builder_ext_handle_t, const void *, size_t, void *, size_t, ze_rtas_parallel_operation_ext_handle_t, void *, ze_rtas_aabb_ext_t *, size_t *)>(
        osLibHandle->funcMap["zeRTASBuilderCommandListAppendCopyExtImpl"]);
    ASSERT_NE(nullptr, func);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, func(hCommandList, hBuilder, pScratchBuffer, scratchBufferSizeBytes, pRtasBuffer, rtasBufferSizeBytes, hParallelOperation, pBuildUserPtr, pBounds, pRtasBufferSizeBytes));
    EXPECT_NE(0u, builderCommandListAppendCopyFailCalled);
}

TEST_F(RTASTestExt, GivenLibraryPreLoadedAndUnderlyingBuilderCreateSucceedsThenSuccessIsReturned_Ext) {
    ze_rtas_builder_ext_handle_t hBuilder;
    builderCreateExtImpl = &builderCreate;
    builderDestroyExtImpl = &builderDestroy;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExt(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(1u, builderCreateCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExt(hBuilder));
    EXPECT_EQ(1u, builderDestroyCalled);
}

TEST_F(RTASTestExt, GivenLibraryPreLoadedAndUnderlyingBuilderCreateFailsThenErrorIsReturned_Ext) {
    ze_rtas_builder_ext_handle_t hBuilder;
    builderCreateExtImpl = &builderCreateFail;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderCreateExt(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(1u, builderCreateFailCalled);
}

TEST_F(RTASTestExt, GivenLibraryFailsToLoadThenBuilderCreateReturnsError_Ext) {
    ze_rtas_builder_ext_handle_t hBuilder;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockRTASExtOsLibrary::load};
    MockRTASExtOsLibrary::mockLoad = false;
    driverHandle->rtasLibraryHandle.reset();
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeRTASBuilderCreateExt(driverHandle->toHandle(), nullptr, &hBuilder));
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderDestroySucceedsThenSuccessIsReturned_Ext) {
    auto pRTASBuilderExt = std::make_unique<RTASBuilderExt>();
    builderDestroyExtImpl = &builderDestroy;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExt(pRTASBuilderExt.release()->toHandle()));
    EXPECT_EQ(1u, builderDestroyCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderDestroyFailsThenErrorIsReturned_Ext) {
    auto pRTASBuilderExt = std::make_unique<RTASBuilderExt>();
    builderDestroyExtImpl = &builderDestroyFail;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderDestroyExt(pRTASBuilderExt.get()->toHandle()));
    EXPECT_EQ(1u, builderDestroyFailCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderGetBuildPropertiesSucceedsThenSuccessIsReturned_Ext) {
    RTASBuilderExt pRTASBuilderExt{};
    builderGetBuildPropertiesExtImpl = &builderGetBuildProperties;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderGetBuildPropertiesExt(pRTASBuilderExt.toHandle(), nullptr, nullptr));
    EXPECT_EQ(1u, builderGetBuildPropertiesCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderGetBuildPropertiesFailsThenErrorIsReturned_Ext) {
    RTASBuilderExt pRTASBuilderExt{};
    builderGetBuildPropertiesExtImpl = &builderGetBuildPropertiesFail;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderGetBuildPropertiesExt(pRTASBuilderExt.toHandle(), nullptr, nullptr));
    EXPECT_EQ(1u, builderGetBuildPropertiesFailCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderBuildSucceedsThenSuccessIsReturned_Ext) {
    RTASBuilderExt pRTASBuilderExt{};
    RTASParallelOperationExt pParallelOperation{};
    builderBuildExtImpl = &builderBuild;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderBuildExt(pRTASBuilderExt.toHandle(),
                                                           nullptr,
                                                           nullptr, 0,
                                                           nullptr, 0,
                                                           pParallelOperation.toHandle(),
                                                           nullptr, nullptr,
                                                           nullptr));
    EXPECT_EQ(1u, builderBuildCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingBuilderBuildFailsThenErrorIsReturned_Ext) {
    RTASBuilderExt pRTASBuilderExt{};
    RTASParallelOperationExt pParallelOperation{};
    builderBuildExtImpl = &builderBuildFail;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderBuildExt(pRTASBuilderExt.toHandle(),
                                                                 nullptr,
                                                                 nullptr, 0,
                                                                 nullptr, 0,
                                                                 pParallelOperation.toHandle(),
                                                                 nullptr, nullptr,
                                                                 nullptr));
    EXPECT_EQ(1u, builderBuildFailCalled);
}

TEST_F(RTASTestExt, GivenLibraryPreLoadedAndUnderlyingFormatCompatibilitySucceedsThenSuccessIsReturned_Ext) {
    formatCompatibilityCheckExtImpl = &formatCompatibilityCheck;
    const ze_rtas_format_ext_t accelFormatA = {};
    const ze_rtas_format_ext_t accelFormatB = {};
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExt(driverHandle->toHandle(), accelFormatA, accelFormatB));
    EXPECT_EQ(1u, formatCompatibilityCheckCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingFormatCompatibilityFailsThenErrorIsReturned_Ext) {
    formatCompatibilityCheckExtImpl = &formatCompatibilityCheckFail;
    const ze_rtas_format_ext_t accelFormatA = {};
    const ze_rtas_format_ext_t accelFormatB = {};
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeDriverRTASFormatCompatibilityCheckExt(driverHandle->toHandle(), accelFormatA, accelFormatB));
    EXPECT_EQ(1u, formatCompatibilityCheckFailCalled);
}

TEST_F(RTASTestExt, GivenLibraryPreLoadedAndUnderlyingParallelOperationCreateSucceedsThenSuccessIsReturned_Ext) {
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation;
    parallelOperationCreateExtImpl = &parallelOperationCreate;
    parallelOperationDestroyExtImpl = &parallelOperationDestroy;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExt(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(1u, parallelOperationCreateCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExt(hParallelOperation));
    EXPECT_EQ(1u, parallelOperationDestroyCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingParallelOperationCreateFailsThenErrorIsReturned_Ext) {
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation;
    parallelOperationCreateExtImpl = &parallelOperationCreateFail;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationCreateExt(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(1u, parallelOperationCreateFailCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingParallelOperationDestroySucceedsThenSuccessIsReturned_Ext) {
    auto pParallelOperation = std::make_unique<RTASParallelOperationExt>();
    parallelOperationDestroyExtImpl = &parallelOperationDestroy;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExt(pParallelOperation.release()->toHandle()));
    EXPECT_EQ(1u, parallelOperationDestroyCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingParallelOperationDestroyFailsThenErrorIsReturned_Ext) {
    auto pParallelOperation = std::make_unique<RTASParallelOperationExt>();
    parallelOperationDestroyExtImpl = &parallelOperationDestroyFail;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationDestroyExt(pParallelOperation.get()->toHandle()));
    EXPECT_EQ(1u, parallelOperationDestroyFailCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingParallelOperationGetPropertiesSucceedsThenSuccessIsReturned_Ext) {
    RTASParallelOperationExt pParallelOperation{};
    parallelOperationGetPropertiesExtImpl = &parallelOperationGetProperties;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationGetPropertiesExt(pParallelOperation.toHandle(), nullptr));
    EXPECT_EQ(1u, parallelOperationGetPropertiesCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingParallelOperationGetPropertiesFailsThenErrorIsReturned_Ext) {
    RTASParallelOperationExt pParallelOperation{};
    parallelOperationGetPropertiesExtImpl = &parallelOperationGetPropertiesFail;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationGetPropertiesExt(pParallelOperation.toHandle(), nullptr));
    EXPECT_EQ(1u, parallelOperationGetPropertiesFailCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingParallelOperationJoinSucceedsThenSuccessIsReturned_Ext) {
    RTASParallelOperationExt pParallelOperation{};
    parallelOperationJoinExtImpl = &parallelOperationJoin;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationJoinExt(pParallelOperation.toHandle()));
    EXPECT_EQ(1u, parallelOperationJoinCalled);
}

TEST_F(RTASTestExt, GivenUnderlyingParallelOperationJoinFailsThenErrorIsReturned_Ext) {
    RTASParallelOperationExt pParallelOperation{};
    parallelOperationJoinExtImpl = &parallelOperationJoinFail;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationJoinExt(pParallelOperation.toHandle()));
    EXPECT_EQ(1u, parallelOperationJoinFailCalled);
}

TEST_F(RTASTestExt, GivenNoSymbolAvailableInLibraryThenLoadEntryPointsReturnsFalse_Ext) {
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
}

TEST_F(RTASTestExt, GivenOnlySingleSymbolAvailableThenLoadEntryPointsReturnsFalse_Ext) {
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    MockRTASExtOsLibrary *osLibHandle = static_cast<MockRTASExtOsLibrary *>(driverHandle->rtasLibraryHandle.get());
    osLibHandle->funcMap["zeRTASBuilderCreateExtImpl"] = reinterpret_cast<void *>(&builderCreate);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderDestroyExtImpl"] = reinterpret_cast<void *>(&builderDestroy);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderGetBuildPropertiesExtImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderBuildExtImpl"] = reinterpret_cast<void *>(&builderBuild);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeDriverRTASFormatCompatibilityCheckExtImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationCreateExtImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationDestroyExtImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationGetPropertiesExtImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationJoinExtImpl"] = reinterpret_cast<void *>(&parallelOperationJoin);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
}

TEST_F(RTASTestExt, GivenMissingSymbolsThenLoadEntryPointsReturnsFalse_Ext) {
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASExtOsLibrary>();
    MockRTASExtOsLibrary *osLibHandle = static_cast<MockRTASExtOsLibrary *>(driverHandle->rtasLibraryHandle.get());
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderCreateExtImpl"] = reinterpret_cast<void *>(&builderCreate);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderDestroyExtImpl"] = reinterpret_cast<void *>(&builderDestroy);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderGetBuildPropertiesExtImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderBuildExtImpl"] = reinterpret_cast<void *>(&builderBuild);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeDriverRTASFormatCompatibilityCheckExtImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASParallelOperationCreateExtImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASParallelOperationDestroyExtImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASParallelOperationGetPropertiesExtImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);
    EXPECT_EQ(false, L0::RTASBuilderExt::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
}

} // namespace ult
} // namespace L0
