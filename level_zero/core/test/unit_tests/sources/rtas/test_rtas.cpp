/*
 * Copyright (C) 2023 Intel Corporation
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
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    static ze_result_t builderCreate(ze_driver_handle_t hDriver,
                                     const ze_rtas_builder_exp_desc_t *pDescriptor,
                                     ze_rtas_builder_exp_handle_t *phBuilder) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderCreateFail(ze_driver_handle_t hDriver,
                                         const ze_rtas_builder_exp_desc_t *pDescriptor,
                                         ze_rtas_builder_exp_handle_t *phBuilder) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderDestroy(ze_rtas_builder_exp_handle_t hBuilder) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderDestroyFail(ze_rtas_builder_exp_handle_t hBuilder) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderGetBuildProperties(ze_rtas_builder_exp_handle_t hBuilder,
                                                 const ze_rtas_builder_build_op_exp_desc_t *args,
                                                 ze_rtas_builder_exp_properties_t *pProp) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderGetBuildPropertiesFail(ze_rtas_builder_exp_handle_t hBuilder,
                                                     const ze_rtas_builder_build_op_exp_desc_t *args,
                                                     ze_rtas_builder_exp_properties_t *pProp) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t builderBuild(ze_rtas_builder_exp_handle_t hBuilder,
                                    const ze_rtas_builder_build_op_exp_desc_t *args,
                                    void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                    void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                    ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                    void *pBuildUserPtr, ze_rtas_aabb_exp_t *pBounds,
                                    size_t *pRtasBufferSizeBytes) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t builderBuildFail(ze_rtas_builder_exp_handle_t hBuilder,
                                        const ze_rtas_builder_build_op_exp_desc_t *args,
                                        void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                        void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                        ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                        void *pBuildUserPtr, ze_rtas_aabb_exp_t *pBounds,
                                        size_t *pRtasBufferSizeBytes) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t formatCompatibilityCheck(ze_driver_handle_t hDriver,
                                                const ze_rtas_format_exp_t accelFormat,
                                                const ze_rtas_format_exp_t otherAccelFormat) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t formatCompatibilityCheckFail(ze_driver_handle_t hDriver,
                                                    const ze_rtas_format_exp_t accelFormat,
                                                    const ze_rtas_format_exp_t otherAccelFormat) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationDestroy(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationDestroyFail(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationCreate(ze_driver_handle_t hDriver,
                                               ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationCreateFail(ze_driver_handle_t hDriver,
                                                   ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationGetProperties(ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                                      ze_rtas_parallel_operation_exp_properties_t *pProperties) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationGetPropertiesFail(ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                                          ze_rtas_parallel_operation_exp_properties_t *pProperties) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    static ze_result_t parallelOperationJoin(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
        return ZE_RESULT_SUCCESS;
    }

    static ze_result_t parallelOperationJoinFail(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
};

using RTASTest = Test<RTASFixture>;

struct MockOsLibrary : public OsLibrary {
  public:
    static bool mockLoad;
    MockOsLibrary(const std::string &name, std::string *errorValue) {
    }
    MockOsLibrary() {}
    ~MockOsLibrary() override = default;
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
    static OsLibrary *load(const std::string &name) {
        if (mockLoad == true) {
            auto ptr = new (std::nothrow) MockOsLibrary();
            return ptr;
        } else {
            return nullptr;
        }
    }
    std::map<std::string, void *> funcMap;
};

bool MockOsLibrary::mockLoad = true;

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
        static OsLibrary *load(const std::string &name) {
            auto ptr = new (std::nothrow) MockSymbolsLoadedOsLibrary();
            return ptr;
        }
        std::map<std::string, void *> funcMap;
    };

    ze_rtas_builder_exp_handle_t hBuilder;
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    L0::RTASBuilder::osLibraryLoadFunction = MockSymbolsLoadedOsLibrary::load;
    driverHandle->rtasLibraryHandle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExp(hBuilder));
    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExp(hParallelOperation));
    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));
    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;
}

TEST_F(RTASTest, GivenLibraryFailedToLoadSymbolsThenErrorIsReturned) {
    ze_rtas_builder_exp_handle_t hBuilder;
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    L0::RTASBuilder::osLibraryLoadFunction = MockOsLibrary::load;
    driverHandle->rtasLibraryHandle = nullptr;

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingBuilderCreateSucceedsThenSuccessIsReturned) {
    ze_rtas_builder_exp_handle_t hBuilder;
    builderCreateExpImpl = &builderCreate;
    builderDestroyExpImpl = &builderDestroy;
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExp(hBuilder));

    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingBuilderCreateFailsThenErrorIsReturned) {
    ze_rtas_builder_exp_handle_t hBuilder;
    builderCreateExpImpl = &builderCreateFail;
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));

    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;
}

TEST_F(RTASTest, GivenLibraryFailsToLoadThenBuilderCreateReturnsError) {
    ze_rtas_builder_exp_handle_t hBuilder;
    L0::RTASBuilder::osLibraryLoadFunction = MockOsLibrary::load;
    MockOsLibrary::mockLoad = false;
    driverHandle->rtasLibraryHandle = nullptr;

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASBuilderCreateExp(driverHandle->toHandle(), nullptr, &hBuilder));
}

TEST_F(RTASTest, GivenUnderlyingBuilderDestroySucceedsThenSuccessIsReturned) {
    RTASBuilder *pRTASBuilder = new RTASBuilder();
    builderDestroyExpImpl = &builderDestroy;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExp(pRTASBuilder->toHandle()));
}

TEST_F(RTASTest, GivenUnderlyingBuilderDestroyFailsThenErrorIsReturned) {
    RTASBuilder *pRTASBuilder = new RTASBuilder();
    builderDestroyExpImpl = &builderDestroyFail;
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASBuilderDestroyExp(pRTASBuilder->toHandle()));

    delete pRTASBuilder;
}

TEST_F(RTASTest, GivenUnderlyingBuilderGetBuildPropertiesSucceedsThenSuccessIsReturned) {
    RTASBuilder *pRTASBuilder = new RTASBuilder();
    builderGetBuildPropertiesExpImpl = &builderGetBuildProperties;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderGetBuildPropertiesExp(pRTASBuilder->toHandle(), nullptr, nullptr));

    delete pRTASBuilder;
}

TEST_F(RTASTest, GivenUnderlyingBuilderGetBuildPropertiesFailsThenErrorIsReturned) {
    RTASBuilder *pRTASBuilder = new RTASBuilder();
    builderGetBuildPropertiesExpImpl = &builderGetBuildPropertiesFail;

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASBuilderGetBuildPropertiesExp(pRTASBuilder->toHandle(), nullptr, nullptr));

    delete pRTASBuilder;
}

TEST_F(RTASTest, GivenUnderlyingBuilderBuildSucceedsThenSuccessIsReturned) {
    RTASBuilder *pRTASBuilder = new RTASBuilder();
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    builderBuildExpImpl = &builderBuild;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASBuilderBuildExp(pRTASBuilder->toHandle(),
                                                           nullptr,
                                                           nullptr, 0,
                                                           nullptr, 0,
                                                           pParallelOperation->toHandle(),
                                                           nullptr, nullptr,
                                                           nullptr));

    delete pParallelOperation;
    delete pRTASBuilder;
}

TEST_F(RTASTest, GivenUnderlyingBuilderBuildFailsThenErrorIsReturned) {
    RTASBuilder *pRTASBuilder = new RTASBuilder();
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    builderBuildExpImpl = &builderBuildFail;

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASBuilderBuildExp(pRTASBuilder->toHandle(),
                                                           nullptr,
                                                           nullptr, 0,
                                                           nullptr, 0,
                                                           pParallelOperation->toHandle(),
                                                           nullptr, nullptr,
                                                           nullptr));

    delete pParallelOperation;
    delete pRTASBuilder;
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingFormatCompatibilitySucceedsThenSuccessIsReturned) {
    formatCompatibilityCheckExpImpl = &formatCompatibilityCheck;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));

    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;
}

TEST_F(RTASTest, GivenUnderlyingFormatCompatibilityFailsThenErrorIsReturned) {
    formatCompatibilityCheckExpImpl = &formatCompatibilityCheckFail;
    const ze_rtas_format_exp_t accelFormatA = {};
    const ze_rtas_format_exp_t accelFormatB = {};
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeDriverRTASFormatCompatibilityCheckExp(driverHandle->toHandle(), accelFormatA, accelFormatB));

    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;
}

TEST_F(RTASTest, GivenLibraryPreLoadedAndUnderlyingParrallelOperationCreateSucceedsThenSuccessIsReturned) {
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    parallelOperationCreateExpImpl = &parallelOperationCreate;
    parallelOperationDestroyExpImpl = &parallelOperationDestroy;
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExp(hParallelOperation));

    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;
}

TEST_F(RTASTest, GivenUnderlyingParrallelOperationCreateFailsThenErrorIsReturned) {
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation;
    parallelOperationCreateExpImpl = &parallelOperationCreateFail;
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationCreateExp(driverHandle->toHandle(), &hParallelOperation));

    delete driverHandle->rtasLibraryHandle;
    driverHandle->rtasLibraryHandle = nullptr;
}

TEST_F(RTASTest, GivenUnderlyingParrallelOperationDestroySucceedsThenSuccessIsReturned) {
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    parallelOperationDestroyExpImpl = &parallelOperationDestroy;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExp(pParallelOperation->toHandle()));
}

TEST_F(RTASTest, GivenUnderlyingParrallelOperationDestroyFailsThenErrorIsReturned) {
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    parallelOperationDestroyExpImpl = &parallelOperationDestroyFail;

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationDestroyExp(pParallelOperation->toHandle()));

    delete pParallelOperation;
}

TEST_F(RTASTest, GivenUnderlyingParrallelOperationGetPropertiesSucceedsThenSuccessIsReturned) {
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    parallelOperationGetPropertiesExpImpl = &parallelOperationGetProperties;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationGetPropertiesExp(pParallelOperation->toHandle(), nullptr));

    delete pParallelOperation;
}

TEST_F(RTASTest, GivenUnderlyingParrallelOperationGetPropertiesFailsThenErrorIsReturned) {
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    parallelOperationGetPropertiesExpImpl = &parallelOperationGetPropertiesFail;

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationGetPropertiesExp(pParallelOperation->toHandle(), nullptr));

    delete pParallelOperation;
}

TEST_F(RTASTest, GivenUnderlyingParrallelOperationJoinSucceedsThenSuccessIsReturned) {
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    parallelOperationJoinExpImpl = &parallelOperationJoin;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationJoinExp(pParallelOperation->toHandle()));

    delete pParallelOperation;
}

TEST_F(RTASTest, GivenUnderlyingParrallelOperationJoinFailsThenErrorIsReturned) {
    RTASParallelOperation *pParallelOperation = new RTASParallelOperation();
    parallelOperationJoinExpImpl = &parallelOperationJoinFail;

    EXPECT_NE(ZE_RESULT_SUCCESS, L0::zeRTASParallelOperationJoinExp(pParallelOperation->toHandle()));

    delete pParallelOperation;
}
TEST_F(RTASTest, GivenNoSymbolAvailableInLibraryThenLoadEntryPointsReturnsFalse) {
    MockOsLibrary *osLibHandle = new MockOsLibrary();
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(osLibHandle);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
}

TEST_F(RTASTest, GivenOnlySingleSymbolAvailableThenLoadEntryPointsReturnsFalse) {
    MockOsLibrary *osLibHandle = new MockOsLibrary();
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(osLibHandle);

    osLibHandle->funcMap["zeRTASBuilderCreateExpImpl"] = reinterpret_cast<void *>(&builderCreate);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderDestroyExpImpl"] = reinterpret_cast<void *>(&builderDestroy);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderGetBuildPropertiesExpImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASBuilderBuildExpImpl"] = reinterpret_cast<void *>(&builderBuild);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeDriverRTASFormatCompatibilityCheckExpImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationCreateExpImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationDestroyExpImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationGetPropertiesExpImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));

    osLibHandle->funcMap.clear();
    osLibHandle->funcMap["zeRTASParallelOperationJoinExpImpl"] = reinterpret_cast<void *>(&parallelOperationJoin);
    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
}

TEST_F(RTASTest, GivenMissingSymbolsThenLoadEntryPointsReturnsFalse) {
    MockOsLibrary *osLibHandle = new MockOsLibrary();
    driverHandle->rtasLibraryHandle = static_cast<OsLibrary *>(osLibHandle);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeRTASBuilderCreateExpImpl"] = reinterpret_cast<void *>(&builderCreate);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeRTASBuilderDestroyExpImpl"] = reinterpret_cast<void *>(&builderDestroy);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeRTASBuilderGetBuildPropertiesExpImpl"] = reinterpret_cast<void *>(&builderGetBuildProperties);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeRTASBuilderBuildExpImpl"] = reinterpret_cast<void *>(&builderBuild);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeDriverRTASFormatCompatibilityCheckExpImpl"] = reinterpret_cast<void *>(&formatCompatibilityCheck);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeRTASParallelOperationCreateExpImpl"] = reinterpret_cast<void *>(&parallelOperationCreate);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeRTASParallelOperationDestroyExpImpl"] = reinterpret_cast<void *>(&parallelOperationDestroy);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
    osLibHandle->funcMap["zeRTASParallelOperationGetPropertiesExpImpl"] = reinterpret_cast<void *>(&parallelOperationGetProperties);

    EXPECT_EQ(false, L0::RTASBuilder::loadEntryPoints(driverHandle->rtasLibraryHandle));
}

} // namespace ult
} // namespace L0