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

struct RTASFixture : public DeviceFixture {
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
                                     const ze_rtas_builder_exp_desc_t *pDescriptor,
                                     ze_rtas_builder_exp_handle_t *phBuilder) {
        builderCreateCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderCreateFail(ze_driver_handle_t hDriver,
                                         const ze_rtas_builder_exp_desc_t *pDescriptor,
                                         ze_rtas_builder_exp_handle_t *phBuilder) {
        builderCreateFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderDestroy(ze_rtas_builder_exp_handle_t hBuilder) {
        builderDestroyCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderDestroyFail(ze_rtas_builder_exp_handle_t hBuilder) {
        builderDestroyFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderGetBuildProperties(ze_rtas_builder_exp_handle_t hBuilder,
                                                 const ze_rtas_builder_build_op_exp_desc_t *args,
                                                 ze_rtas_builder_exp_properties_t *pProp) {
        builderGetBuildPropertiesCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderGetBuildPropertiesFail(ze_rtas_builder_exp_handle_t hBuilder,
                                                     const ze_rtas_builder_build_op_exp_desc_t *args,
                                                     ze_rtas_builder_exp_properties_t *pProp) {
        builderGetBuildPropertiesFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderBuild(ze_rtas_builder_exp_handle_t hBuilder,
                                    const ze_rtas_builder_build_op_exp_desc_t *args,
                                    void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                    void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                    ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                    void *pBuildUserPtr, ze_rtas_aabb_exp_t *pBounds,
                                    size_t *pRtasBufferSizeBytes) {
        builderBuildCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderBuildFail(ze_rtas_builder_exp_handle_t hBuilder,
                                        const ze_rtas_builder_build_op_exp_desc_t *args,
                                        void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                        void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                        ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                        void *pBuildUserPtr, ze_rtas_aabb_exp_t *pBounds,
                                        size_t *pRtasBufferSizeBytes) {
        builderBuildFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t formatCompatibilityCheck(ze_driver_handle_t hDriver,
                                                const ze_rtas_format_exp_t accelFormat,
                                                const ze_rtas_format_exp_t otherAccelFormat) {
        formatCompatibilityCheckCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t formatCompatibilityCheckFail(ze_driver_handle_t hDriver,
                                                    const ze_rtas_format_exp_t accelFormat,
                                                    const ze_rtas_format_exp_t otherAccelFormat) {
        formatCompatibilityCheckFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationDestroy(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
        parallelOperationDestroyCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationDestroyFail(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
        parallelOperationDestroyFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationCreate(ze_driver_handle_t hDriver,
                                               ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) {
        parallelOperationCreateCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationCreateFail(ze_driver_handle_t hDriver,
                                                   ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) {
        parallelOperationCreateFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationGetProperties(ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                                      ze_rtas_parallel_operation_exp_properties_t *pProperties) {
        parallelOperationGetPropertiesCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationGetPropertiesFail(ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                                          ze_rtas_parallel_operation_exp_properties_t *pProperties) {
        parallelOperationGetPropertiesFailCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationJoin(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
        parallelOperationJoinCalled++;
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationJoinFail(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
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

uint32_t RTASFixture::builderCreateCalled = 0;
uint32_t RTASFixture::builderCreateFailCalled = 0;
uint32_t RTASFixture::builderDestroyCalled = 0;
uint32_t RTASFixture::builderDestroyFailCalled = 0;
uint32_t RTASFixture::builderGetBuildPropertiesCalled = 0;
uint32_t RTASFixture::builderGetBuildPropertiesFailCalled = 0;
uint32_t RTASFixture::builderBuildCalled = 0;
uint32_t RTASFixture::builderBuildFailCalled = 0;
uint32_t RTASFixture::formatCompatibilityCheckCalled = 0;
uint32_t RTASFixture::formatCompatibilityCheckFailCalled = 0;
uint32_t RTASFixture::parallelOperationDestroyCalled = 0;
uint32_t RTASFixture::parallelOperationDestroyFailCalled = 0;
uint32_t RTASFixture::parallelOperationCreateCalled = 0;
uint32_t RTASFixture::parallelOperationCreateFailCalled = 0;
uint32_t RTASFixture::parallelOperationGetPropertiesCalled = 0;
uint32_t RTASFixture::parallelOperationGetPropertiesFailCalled = 0;
uint32_t RTASFixture::parallelOperationJoinCalled = 0;
uint32_t RTASFixture::parallelOperationJoinFailCalled = 0;

using RTASTest = Test<RTASFixture>;

struct MockRTASOsLibrary : public OsLibrary {
  public:
    static bool mockLoad;
    MockRTASOsLibrary(const std::string &name, std::string *errorValue) {
    }
    MockRTASOsLibrary() {}
    ~MockRTASOsLibrary() override = default;
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
            auto ptr = new (std::nothrow) MockRTASOsLibrary();
            return ptr;
        } else {
            return nullptr;
        }
    }
    std::map<std::string, void *> funcMap;
};

bool MockRTASOsLibrary::mockLoad = true;

TEST_F(RTASTest, GivenLibraryLoadsSymbolsAndUnderlyingFunctionsSucceedThenSuccessIsReturned) {
    struct MockSymbolsLoadedOsLibrary : public OsLibrary {
      public:
        MockSymbolsLoadedOsLibrary(const std::string &name, std::string *errorValue) {
        }
        MockSymbolsLoadedOsLibrary() {}
        ~MockSymbolsLoadedOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            funcMap["zeRTASBuilderCreateExpImpl"] = reinterpret_cast<void *>(&builderCreate);
            funcMap["zeRTASBuilderDestroyExpImpl"] = reinterpret_cast<void *>(&builderDestroy);
            funcMap["zeRTASBuilderGetBuildPropertiesExpImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);
            funcMap["zeRTASBuilderBuildExpImpl"] = reinterpret_cast<void *>(&builderBuild);
            funcMap["zeDriverRTASFormatCompatibilityCheckExpImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);
            funcMap["zeRTASParallelOperationCreateExpImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);
            funcMap["zeRTASParallelOperationDestroyExpImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);
            funcMap["zeRTASParallelOperationGetPropertiesExpImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);
            funcMap["zeRTASParallelOperationJoinExpImpl"] = reinterpret_cast<void *>(&parallelOperationJoin);
            auto it = funcMap.find(procName);
            if (funcMap.end() == it) {
                return nullptr;
            } else {
                return it->second;
            }
        }
        bool isLoaded() override {
            return true;
        }
        std::string getFullPath() override {
            return std::string();
        }
        static OsLibrary *load(const OsLibraryCreateProperties &properties) {
            auto ptr = new (std::nothrow) MockSymbolsLoadedOsLibrary();
            return ptr;
        }
        std::map<std::string, void *> funcMap;
    };

    ze_rtas_builder_exp_handle_t hBuilder;
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockSymbolsLoadedOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(1u, builderCreateCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExp(hBuilder));
    EXPECT_EQ(1u, builderDestroyCalled);
    driverHandle->rtasLibraryHandle.reset();

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(1u, parallelOperationCreateCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExp(hParallelOperation));
    EXPECT_EQ(1u, parallelOperationDestroyCalled);
    driverHandle->rtasLibraryHandle.reset();

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));
    EXPECT_EQ(1u, formatCompatibilityCheckCalled);

    driverHandle->rtasLibraryHandle.reset();
}

TEST_F(RTASTest, GivenLibraryFailedToLoadSymbolsThenErrorIsReturned) {
    ze_rtas_builder_exp_handle_t hBuilder;
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockRTASOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingBuilderCreateSucceedsThenSuccessIsReturned) {
    ze_rtas_builder_exp_handle_t hBuilder;
    builderCreateExpImpl = &builderCreate;
    builderDestroyExpImpl = &builderDestroy;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(1u, builderCreateCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExp(hBuilder));
    EXPECT_EQ(1u, builderDestroyCalled);
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingBuilderCreateFailsThenErrorIsReturned) {
    ze_rtas_builder_exp_handle_t hBuilder;
    builderCreateExpImpl = &builderCreateFail;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(1u, builderCreateFailCalled);
}

TEST_F(RTASTest, GivenLibraryFailsToLoadThenBuilderCreateReturnsError) {
    ze_rtas_builder_exp_handle_t hBuilder;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockRTASOsLibrary::load};
    MockRTASOsLibrary::mockLoad = false;
    driverHandle->rtasLibraryHandle.reset();

    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
}

TEST_F(RTASTest, GivenUnderlyingBuilderDestroySucceedsThenSuccessIsReturned) {
    auto pRTASBuilder = std::make_unique<RTASBuilder>();
    builderDestroyExpImpl = &builderDestroy;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExp(pRTASBuilder.release()));
    EXPECT_EQ(1u, builderDestroyCalled);
}

TEST_F(RTASTest, GivenUnderlyingBuilderDestroyFailsThenErrorIsReturned) {
    auto pRTASBuilder = std::make_unique<RTASBuilder>();
    builderDestroyExpImpl = &builderDestroyFail;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderDestroyExp(pRTASBuilder.get()));
    EXPECT_EQ(1u, builderDestroyFailCalled);
}

TEST_F(RTASTest, GivenUnderlyingBuilderGetBuildPropertiesSucceedsThenSuccessIsReturned) {
    RTASBuilder pRTASBuilder{};
    builderGetBuildPropertiesExpImpl = &builderGetBuildProperties;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderGetBuildPropertiesExp(pRTASBuilder.toHandle(), nullptr, nullptr));
    EXPECT_EQ(1u, builderGetBuildPropertiesCalled);
}

TEST_F(RTASTest, GivenUnderlyingBuilderGetBuildPropertiesFailsThenErrorIsReturned) {
    RTASBuilder pRTASBuilder{};
    builderGetBuildPropertiesExpImpl = &builderGetBuildPropertiesFail;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderGetBuildPropertiesExp(pRTASBuilder.toHandle(), nullptr, nullptr));
    EXPECT_EQ(1u, builderGetBuildPropertiesFailCalled);
}

TEST_F(RTASTest, GivenUnderlyingBuilderBuildSucceedsThenSuccessIsReturned) {
    RTASBuilder pRTASBuilder{};
    RTASParallelOperation pParallelOperation{};
    builderBuildExpImpl = &builderBuild;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderBuildExp(pRTASBuilder.toHandle(),
                                                           nullptr,
                                                           nullptr, 0,
                                                           nullptr, 0,
                                                           pParallelOperation.toHandle(),
                                                           nullptr, nullptr,
                                                           nullptr));
    EXPECT_EQ(1u, builderBuildCalled);
}

TEST_F(RTASTest, GivenUnderlyingBuilderBuildFailsThenErrorIsReturned) {
    RTASBuilder pRTASBuilder{};
    RTASParallelOperation pParallelOperation{};
    builderBuildExpImpl = &builderBuildFail;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASBuilderBuildExp(pRTASBuilder.toHandle(),
                                                                 nullptr,
                                                                 nullptr, 0,
                                                                 nullptr, 0,
                                                                 pParallelOperation.toHandle(),
                                                                 nullptr, nullptr,
                                                                 nullptr));
    EXPECT_EQ(1u, builderBuildFailCalled);
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingFormatCompatibilitySucceedsThenSuccessIsReturned) {
    formatCompatibilityCheckExpImpl = &formatCompatibilityCheck;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));
    EXPECT_EQ(1u, formatCompatibilityCheckCalled);
}

TEST_F(RTASTest, GivenUnderlyingFormatCompatibilityFailsThenErrorIsReturned) {
    formatCompatibilityCheckExpImpl = &formatCompatibilityCheckFail;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));
    EXPECT_EQ(1u, formatCompatibilityCheckFailCalled);
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingParallelOperationCreateSucceedsThenSuccessIsReturned) {
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    parallelOperationCreateExpImpl = &parallelOperationCreate;
    parallelOperationDestroyExpImpl = &parallelOperationDestroy;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(1u, parallelOperationCreateCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExp(hParallelOperation));
    EXPECT_EQ(1u, parallelOperationDestroyCalled);
}

TEST_F(RTASTest, GivenUnderlyingParallelOperationCreateFailsThenErrorIsReturned) {
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    parallelOperationCreateExpImpl = &parallelOperationCreateFail;
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(1u, parallelOperationCreateFailCalled);
}

TEST_F(RTASTest, GivenUnderlyingParallelOperationDestroySucceedsThenSuccessIsReturned) {
    auto pParallelOperation = std::make_unique<RTASParallelOperation>();
    parallelOperationDestroyExpImpl = &parallelOperationDestroy;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExp(pParallelOperation.release()));
    EXPECT_EQ(1u, parallelOperationDestroyCalled);
}

TEST_F(RTASTest, GivenUnderlyingParallelOperationDestroyFailsThenErrorIsReturned) {
    auto pParallelOperation = std::make_unique<RTASParallelOperation>();
    parallelOperationDestroyExpImpl = &parallelOperationDestroyFail;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationDestroyExp(pParallelOperation.get()));
    EXPECT_EQ(1u, parallelOperationDestroyFailCalled);
}

TEST_F(RTASTest, GivenUnderlyingParallelOperationGetPropertiesSucceedsThenSuccessIsReturned) {
    RTASParallelOperation pParallelOperation{};
    parallelOperationGetPropertiesExpImpl = &parallelOperationGetProperties;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationGetPropertiesExp(pParallelOperation.toHandle(), nullptr));
    EXPECT_EQ(1u, parallelOperationGetPropertiesCalled);
}

TEST_F(RTASTest, GivenUnderlyingParallelOperationGetPropertiesFailsThenErrorIsReturned) {
    RTASParallelOperation pParallelOperation{};
    parallelOperationGetPropertiesExpImpl = &parallelOperationGetPropertiesFail;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationGetPropertiesExp(pParallelOperation.toHandle(), nullptr));
    EXPECT_EQ(1u, parallelOperationGetPropertiesFailCalled);
}

TEST_F(RTASTest, GivenUnderlyingParallelOperationJoinSucceedsThenSuccessIsReturned) {
    RTASParallelOperation pParallelOperation{};
    parallelOperationJoinExpImpl = &parallelOperationJoin;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationJoinExp(pParallelOperation.toHandle()));
    EXPECT_EQ(1u, parallelOperationJoinCalled);
}

TEST_F(RTASTest, GivenUnderlyingParallelOperationJoinFailsThenErrorIsReturned) {
    RTASParallelOperation pParallelOperation{};
    parallelOperationJoinExpImpl = &parallelOperationJoinFail;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, L0::zeRTASParallelOperationJoinExp(pParallelOperation.toHandle()));
    EXPECT_EQ(1u, parallelOperationJoinFailCalled);
}

TEST_F(RTASTest, GivenNoSymbolAvailableInLibraryThenLoadEntryPointsReturnsFalse) {
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
}

TEST_F(RTASTest, GivenRTASLibraryHandleUnavailableThenDependencyUnavailableErrorIsReturned) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockRTASOsLibrary::load};
    MockRTASOsLibrary::mockLoad = false;
    driverHandle->rtasLibraryHandle.reset();

    EXPECT_EQ(false, driverHandle->rtasLibraryUnavailable);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, driverHandle->loadRTASLibrary());
    EXPECT_EQ(true, driverHandle->rtasLibraryUnavailable);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, driverHandle->loadRTASLibrary());
}

TEST_F(RTASTest, GivenOnlySingleSymbolAvailableThenLoadEntryPointsReturnsFalse) {
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();
    MockRTASOsLibrary *osLibHandle = static_cast<MockRTASOsLibrary *>(driverHandle->rtasLibraryHandle.get());

    osLibHandle->funcMap["zeRTASBuilderCreateExpImpl"] = reinterpret_cast<void *>(&builderCreate);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderDestroyExpImpl"] = reinterpret_cast<void *>(&builderDestroy);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderGetBuildPropertiesExpImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderBuildExpImpl"] = reinterpret_cast<void *>(&builderBuild);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeDriverRTASFormatCompatibilityCheckExpImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationCreateExpImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationDestroyExpImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationGetPropertiesExpImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationJoinExpImpl"] = reinterpret_cast<void *>(&parallelOperationJoin);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
}

TEST_F(RTASTest, GivenMissingSymbolsThenLoadEntryPointsReturnsFalse) {
    driverHandle->rtasLibraryHandle = std::make_unique<MockRTASOsLibrary>();
    MockRTASOsLibrary *osLibHandle = static_cast<MockRTASOsLibrary *>(driverHandle->rtasLibraryHandle.get());

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderCreateExpImpl"] = reinterpret_cast<void *>(&builderCreate);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderDestroyExpImpl"] = reinterpret_cast<void *>(&builderDestroy);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderGetBuildPropertiesExpImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASBuilderBuildExpImpl"] = reinterpret_cast<void *>(&builderBuild);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeDriverRTASFormatCompatibilityCheckExpImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASParallelOperationCreateExpImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASParallelOperationDestroyExpImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
    osLibHandle->funcMap["zeRTASParallelOperationGetPropertiesExpImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle.get()));
}

} // namespace ult
} // namespace L0
