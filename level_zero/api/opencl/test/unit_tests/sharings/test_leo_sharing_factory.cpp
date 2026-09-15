/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.inl"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "CL/cl.h"

#include <array>
#include <memory>
#include <string>

namespace NEO {
namespace LEO {
namespace ult {

struct MockSharingBuilderFactory;

struct MockSharingContextBuilder : public SharingContextBuilder {
    MockSharingContextBuilder(MockSharingBuilderFactory *owner) : owner(owner) {}

    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;

    MockSharingBuilderFactory *owner = nullptr;
};

struct MockSharingBuilderFactory : public SharingBuilderFactory {
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override {
        createContextBuilderCalled++;
        auto builder = std::make_unique<MockSharingContextBuilder>(this);
        lastCreatedBuilder = builder.get();
        return builder;
    }

    std::string getExtensions(DriverInfo *driverInfo) override {
        getExtensionsCalled++;
        driverInfoPassedToGetExtensions = driverInfo;
        return extensions;
    }

    void fillGlobalDispatchTable() override {
        fillGlobalDispatchTableCalled++;
    }

    void *getExtensionFunctionAddress(const std::string &functionName) override {
        getExtensionFunctionAddressCalled++;
        functionNameQueried = functionName;
        return extensionFunctionAddress;
    }

    void setExtensionEnabled(DriverInfo *driverInfo) override {
        setExtensionEnabledCalled++;
        driverInfoPassedToSetExtensionEnabled = driverInfo;
    }

    std::string extensions{};
    void *extensionFunctionAddress = nullptr;
    std::string functionNameQueried{};
    DriverInfo *driverInfoPassedToGetExtensions = nullptr;
    DriverInfo *driverInfoPassedToSetExtensionEnabled = nullptr;
    MockSharingContextBuilder *lastCreatedBuilder = nullptr;

    uint32_t createContextBuilderCalled = 0u;
    uint32_t getExtensionsCalled = 0u;
    uint32_t fillGlobalDispatchTableCalled = 0u;
    uint32_t getExtensionFunctionAddressCalled = 0u;
    uint32_t setExtensionEnabledCalled = 0u;

    uint32_t processPropertiesCalled = 0u;
    uint32_t finalizePropertiesCalled = 0u;
    cl_context_properties propertyTypeSeen = 0;
    cl_context_properties propertyValueSeen = 0;
    bool processPropertiesResult = false;
    bool finalizePropertiesResult = true;
    int32_t errcodeToReport = CL_SUCCESS;
};

bool MockSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) {
    owner->processPropertiesCalled++;
    owner->propertyTypeSeen = propertyType;
    owner->propertyValueSeen = propertyValue;
    return owner->processPropertiesResult;
}

bool MockSharingContextBuilder::finalizeProperties(Context &, int32_t &errcodeRet) {
    owner->finalizePropertiesCalled++;
    errcodeRet = owner->errcodeToReport;
    return owner->finalizePropertiesResult;
}

struct DefaultExtensionEnabledFactory : public SharingBuilderFactory {
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override { return nullptr; }
    std::string getExtensions(DriverInfo *) override { return {}; }
    void *getExtensionFunctionAddress(const std::string &) override { return nullptr; }
};

struct SharingFactoryHelper : public SharingFactory {
    static SharingBuilderFactory **builders() { return sharingContextBuilder; }
};

struct SharingFactoryFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();

        auto builders = SharingFactoryHelper::builders();
        for (size_t i = 0; i < static_cast<size_t>(SharingType::MAX_SHARING_VALUE); i++) {
            savedBuilders[i] = builders[i];
            builders[i] = nullptr;
        }

        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();

        auto builders = SharingFactoryHelper::builders();
        for (size_t i = 0; i < static_cast<size_t>(SharingType::MAX_SHARING_VALUE); i++) {
            delete builders[i];
            builders[i] = savedBuilders[i];
        }

        Test<OclFixture>::TearDown();
    }

    MockSharingBuilderFactory *installMock(SharingType type) {
        auto mock = new MockSharingBuilderFactory();
        SharingFactoryHelper::builders()[type] = mock;
        return mock;
    }

    size_t countRegisteredBuilders() const {
        size_t registered = 0u;
        auto builders = SharingFactoryHelper::builders();
        for (size_t i = 0; i < static_cast<size_t>(SharingType::MAX_SHARING_VALUE); i++) {
            if (builders[i] != nullptr) {
                registered++;
            }
        }
        return registered;
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
    std::array<SharingBuilderFactory *, SharingType::MAX_SHARING_VALUE> savedBuilders{};
};

TEST_F(SharingFactoryFixture, givenNoRegisteredBuildersWhenBuildingThenFactoryIsCreatedWithoutSharings) {
    auto factory = SharingFactory::build();
    ASSERT_NE(nullptr, factory);

    cl_context_properties propertyType = CL_CONTEXT_PLATFORM;
    cl_context_properties propertyValue = 0;
    EXPECT_FALSE(factory->processProperties(propertyType, propertyValue));

    int32_t errcode = CL_INVALID_VALUE;
    EXPECT_TRUE(factory->finalizeProperties(*leoContext, errcode));
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST_F(SharingFactoryFixture, givenRegisteredBuilderWhenBuildingThenContextBuilderIsRequestedOnce) {
    auto mock = installMock(SharingType::VA_SHARING);

    auto factory = SharingFactory::build();
    ASSERT_NE(nullptr, factory);
    EXPECT_EQ(1u, mock->createContextBuilderCalled);
    EXPECT_NE(nullptr, mock->lastCreatedBuilder);
}

TEST_F(SharingFactoryFixture, givenSeveralRegisteredBuildersWhenBuildingThenEachIsAskedForItsContextBuilder) {
    auto first = installMock(SharingType::CLGL_SHARING);
    auto second = installMock(SharingType::D3D11_SHARING);
    auto third = installMock(SharingType::UNIFIED_SHARING);

    auto factory = SharingFactory::build();
    ASSERT_NE(nullptr, factory);
    EXPECT_EQ(1u, first->createContextBuilderCalled);
    EXPECT_EQ(1u, second->createContextBuilderCalled);
    EXPECT_EQ(1u, third->createContextBuilderCalled);
}

TEST_F(SharingFactoryFixture, givenRegisteredBuilderWhenBuildingTwiceThenEachFactoryGetsItsOwnContextBuilder) {
    auto mock = installMock(SharingType::VA_SHARING);

    auto first = SharingFactory::build();
    auto firstBuilder = mock->lastCreatedBuilder;
    auto second = SharingFactory::build();

    EXPECT_EQ(2u, mock->createContextBuilderCalled);
    EXPECT_NE(firstBuilder, mock->lastCreatedBuilder);
}

TEST_F(SharingFactoryFixture, givenBuilderRejectingPropertyWhenProcessingPropertiesThenReturnsFalseAndPropertyIsForwarded) {
    auto mock = installMock(SharingType::VA_SHARING);
    mock->processPropertiesResult = false;

    auto factory = SharingFactory::build();
    cl_context_properties propertyType = 0x4242;
    cl_context_properties propertyValue = 0x1234;

    EXPECT_FALSE(factory->processProperties(propertyType, propertyValue));
    EXPECT_EQ(1u, mock->processPropertiesCalled);
    EXPECT_EQ(0x4242, mock->propertyTypeSeen);
    EXPECT_EQ(0x1234, mock->propertyValueSeen);
}

TEST_F(SharingFactoryFixture, givenBuilderAcceptingPropertyWhenProcessingPropertiesThenReturnsTrue) {
    auto mock = installMock(SharingType::VA_SHARING);
    mock->processPropertiesResult = true;

    auto factory = SharingFactory::build();
    cl_context_properties propertyType = 0x4242;
    cl_context_properties propertyValue = 0;

    EXPECT_TRUE(factory->processProperties(propertyType, propertyValue));
    EXPECT_EQ(1u, mock->processPropertiesCalled);
}

TEST_F(SharingFactoryFixture, givenFirstBuilderAcceptingPropertyWhenProcessingPropertiesThenRemainingBuildersAreNotAsked) {
    auto first = installMock(SharingType::CLGL_SHARING);
    auto second = installMock(SharingType::D3D11_SHARING);
    first->processPropertiesResult = true;
    second->processPropertiesResult = true;

    auto factory = SharingFactory::build();
    cl_context_properties propertyType = 0x4242;
    cl_context_properties propertyValue = 0;

    EXPECT_TRUE(factory->processProperties(propertyType, propertyValue));
    EXPECT_EQ(1u, first->processPropertiesCalled);
    EXPECT_EQ(0u, second->processPropertiesCalled);
}

TEST_F(SharingFactoryFixture, givenOnlyLastBuilderAcceptingPropertyWhenProcessingPropertiesThenAllAreAskedInOrder) {
    auto first = installMock(SharingType::CLGL_SHARING);
    auto second = installMock(SharingType::D3D11_SHARING);
    first->processPropertiesResult = false;
    second->processPropertiesResult = true;

    auto factory = SharingFactory::build();
    cl_context_properties propertyType = 0x4242;
    cl_context_properties propertyValue = 0;

    EXPECT_TRUE(factory->processProperties(propertyType, propertyValue));
    EXPECT_EQ(1u, first->processPropertiesCalled);
    EXPECT_EQ(1u, second->processPropertiesCalled);
}

TEST_F(SharingFactoryFixture, givenAllBuildersSucceedingWhenFinalizingPropertiesThenReturnsTrueAndAllAreCalled) {
    auto first = installMock(SharingType::CLGL_SHARING);
    auto second = installMock(SharingType::VA_SHARING);
    first->finalizePropertiesResult = true;
    second->finalizePropertiesResult = true;

    auto factory = SharingFactory::build();
    int32_t errcode = CL_SUCCESS;

    EXPECT_TRUE(factory->finalizeProperties(*leoContext, errcode));
    EXPECT_EQ(1u, first->finalizePropertiesCalled);
    EXPECT_EQ(1u, second->finalizePropertiesCalled);
    EXPECT_EQ(CL_SUCCESS, errcode);
}

TEST_F(SharingFactoryFixture, givenFirstBuilderFailingWhenFinalizingPropertiesThenReturnsFalseAndRemainingAreSkipped) {
    auto first = installMock(SharingType::CLGL_SHARING);
    auto second = installMock(SharingType::VA_SHARING);
    first->finalizePropertiesResult = false;
    first->errcodeToReport = CL_INVALID_OPERATION;

    auto factory = SharingFactory::build();
    int32_t errcode = CL_SUCCESS;

    EXPECT_FALSE(factory->finalizeProperties(*leoContext, errcode));
    EXPECT_EQ(CL_INVALID_OPERATION, errcode);
    EXPECT_EQ(1u, first->finalizePropertiesCalled);
    EXPECT_EQ(0u, second->finalizePropertiesCalled);
}

TEST_F(SharingFactoryFixture, givenLastBuilderFailingWhenFinalizingPropertiesThenReturnsFalseAfterCallingAll) {
    auto first = installMock(SharingType::CLGL_SHARING);
    auto second = installMock(SharingType::VA_SHARING);
    second->finalizePropertiesResult = false;
    second->errcodeToReport = CL_OUT_OF_RESOURCES;

    auto factory = SharingFactory::build();
    int32_t errcode = CL_SUCCESS;

    EXPECT_FALSE(factory->finalizeProperties(*leoContext, errcode));
    EXPECT_EQ(CL_OUT_OF_RESOURCES, errcode);
    EXPECT_EQ(1u, first->finalizePropertiesCalled);
    EXPECT_EQ(1u, second->finalizePropertiesCalled);
}

TEST_F(SharingFactoryFixture, givenNoRegisteredBuildersWhenQueryingExtensionsThenResultIsEmpty) {
    EXPECT_TRUE(sharingFactory.getExtensions(nullptr).empty());
}

TEST_F(SharingFactoryFixture, givenRegisteredBuildersWhenQueryingExtensionsThenTheyAreConcatenatedInSharingTypeOrder) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(false);

    auto gl = installMock(SharingType::CLGL_SHARING);
    auto va = installMock(SharingType::VA_SHARING);
    gl->extensions = "cl_khr_gl_sharing ";
    va->extensions = "cl_intel_va_api_media_sharing ";

    EXPECT_STREQ("cl_khr_gl_sharing cl_intel_va_api_media_sharing ", sharingFactory.getExtensions(nullptr).c_str());
    EXPECT_EQ(1u, gl->getExtensionsCalled);
    EXPECT_EQ(1u, va->getExtensionsCalled);
}

TEST_F(SharingFactoryFixture, givenDriverInfoWhenQueryingExtensionsThenItIsForwardedToEveryBuilder) {
    auto gl = installMock(SharingType::CLGL_SHARING);
    auto va = installMock(SharingType::VA_SHARING);
    auto driverInfo = reinterpret_cast<DriverInfo *>(0x1234u);

    sharingFactory.getExtensions(driverInfo);

    EXPECT_EQ(driverInfo, gl->driverInfoPassedToGetExtensions);
    EXPECT_EQ(driverInfo, va->driverInfoPassedToGetExtensions);
}

TEST_F(SharingFactoryFixture, givenFormatQueryEnabledAndAvailableSharingWhenQueryingExtensionsThenFormatQueryIsAppended) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(true);

    auto gl = installMock(SharingType::CLGL_SHARING);
    gl->extensions = "cl_khr_gl_sharing ";

    const std::string expected = std::string{"cl_khr_gl_sharing "} + Extensions::sharingFormatQuery;
    EXPECT_EQ(expected, sharingFactory.getExtensions(nullptr));
}

TEST_F(SharingFactoryFixture, givenFormatQueryEnabledAndNoSharingWhenQueryingExtensionsThenFormatQueryIsNotAppended) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(true);

    EXPECT_TRUE(sharingFactory.getExtensions(nullptr).empty());
}

TEST_F(SharingFactoryFixture, givenFormatQueryDisabledAndAvailableSharingWhenQueryingExtensionsThenFormatQueryIsNotAppended) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(false);

    auto gl = installMock(SharingType::CLGL_SHARING);
    gl->extensions = "cl_khr_gl_sharing ";

    EXPECT_STREQ("cl_khr_gl_sharing ", sharingFactory.getExtensions(nullptr).c_str());
}

TEST_F(SharingFactoryFixture, givenRegisteredBuildersWhenFillingGlobalDispatchTableThenEveryBuilderIsAsked) {
    auto gl = installMock(SharingType::CLGL_SHARING);
    auto d3d = installMock(SharingType::D3D9_SHARING);

    sharingFactory.fillGlobalDispatchTable();

    EXPECT_EQ(1u, gl->fillGlobalDispatchTableCalled);
    EXPECT_EQ(1u, d3d->fillGlobalDispatchTableCalled);
}

TEST_F(SharingFactoryFixture, givenNoRegisteredBuildersWhenQueryingExtensionFunctionAddressThenReturnsNull) {
    EXPECT_EQ(nullptr, sharingFactory.getExtensionFunctionAddress("clSomeExtensionINTEL"));
}

TEST_F(SharingFactoryFixture, givenBuilderProvidingTheFunctionWhenQueryingExtensionFunctionAddressThenItIsReturned) {
    auto gl = installMock(SharingType::CLGL_SHARING);
    auto expectedAddress = reinterpret_cast<void *>(0xABCDu);
    gl->extensionFunctionAddress = expectedAddress;

    EXPECT_EQ(expectedAddress, sharingFactory.getExtensionFunctionAddress("clSomeExtensionINTEL"));
    EXPECT_STREQ("clSomeExtensionINTEL", gl->functionNameQueried.c_str());
}

TEST_F(SharingFactoryFixture, givenFirstBuilderProvidingTheFunctionWhenQueryingExtensionFunctionAddressThenRemainingAreNotAsked) {
    auto gl = installMock(SharingType::CLGL_SHARING);
    auto va = installMock(SharingType::VA_SHARING);
    gl->extensionFunctionAddress = reinterpret_cast<void *>(0xABCDu);
    va->extensionFunctionAddress = reinterpret_cast<void *>(0xDCBAu);

    EXPECT_EQ(gl->extensionFunctionAddress, sharingFactory.getExtensionFunctionAddress("clSomeExtensionINTEL"));
    EXPECT_EQ(1u, gl->getExtensionFunctionAddressCalled);
    EXPECT_EQ(0u, va->getExtensionFunctionAddressCalled);
}

TEST_F(SharingFactoryFixture, givenOnlyLastBuilderProvidingTheFunctionWhenQueryingExtensionFunctionAddressThenAllAreAsked) {
    auto gl = installMock(SharingType::CLGL_SHARING);
    auto va = installMock(SharingType::VA_SHARING);
    va->extensionFunctionAddress = reinterpret_cast<void *>(0xDCBAu);

    EXPECT_EQ(va->extensionFunctionAddress, sharingFactory.getExtensionFunctionAddress("clSomeExtensionINTEL"));
    EXPECT_EQ(1u, gl->getExtensionFunctionAddressCalled);
    EXPECT_EQ(1u, va->getExtensionFunctionAddressCalled);
}

TEST_F(SharingFactoryFixture, givenNoBuilderProvidingTheFunctionWhenQueryingExtensionFunctionAddressThenReturnsNullAfterAskingAll) {
    auto gl = installMock(SharingType::CLGL_SHARING);
    auto va = installMock(SharingType::VA_SHARING);

    EXPECT_EQ(nullptr, sharingFactory.getExtensionFunctionAddress("clSomeExtensionINTEL"));
    EXPECT_EQ(1u, gl->getExtensionFunctionAddressCalled);
    EXPECT_EQ(1u, va->getExtensionFunctionAddressCalled);
}

TEST_F(SharingFactoryFixture, givenRegisteredBuildersWhenVerifyingExtensionSupportThenDriverInfoReachesEveryBuilder) {
    auto gl = installMock(SharingType::CLGL_SHARING);
    auto va = installMock(SharingType::VA_SHARING);
    auto driverInfo = reinterpret_cast<DriverInfo *>(0x4321u);

    sharingFactory.verifyExtensionSupport(driverInfo);

    EXPECT_EQ(1u, gl->setExtensionEnabledCalled);
    EXPECT_EQ(1u, va->setExtensionEnabledCalled);
    EXPECT_EQ(driverInfo, gl->driverInfoPassedToSetExtensionEnabled);
    EXPECT_EQ(driverInfo, va->driverInfoPassedToSetExtensionEnabled);
}

TEST_F(SharingFactoryFixture, givenBuilderNotOverridingSetExtensionEnabledWhenVerifyingExtensionSupportThenNothingHappens) {
    SharingFactoryHelper::builders()[SharingType::UNIFIED_SHARING] = new DefaultExtensionEnabledFactory();

    sharingFactory.verifyExtensionSupport(nullptr);

    EXPECT_EQ(1u, countRegisteredBuilders());
}

TEST_F(SharingFactoryFixture, givenRegisteredBuildersWhenClearingSharingBuildersThenAllSlotsBecomeEmpty) {
    installMock(SharingType::CLGL_SHARING);
    installMock(SharingType::VA_SHARING);
    ASSERT_EQ(2u, countRegisteredBuilders());

    SharingFactory::clearSharingBuilders();

    EXPECT_EQ(0u, countRegisteredBuilders());
    EXPECT_TRUE(sharingFactory.getExtensions(nullptr).empty());
}

TEST_F(SharingFactoryFixture, givenNoRegisteredBuildersWhenClearingSharingBuildersThenNothingHappens) {
    ASSERT_EQ(0u, countRegisteredBuilders());
    SharingFactory::clearSharingBuilders();
    EXPECT_EQ(0u, countRegisteredBuilders());
}

struct FakeVaSharing {
    static constexpr uint32_t sharingId = SharingType::VA_SHARING;
};

struct FakeUnifiedSharing {
    static constexpr uint32_t sharingId = SharingType::UNIFIED_SHARING;
};

TEST_F(SharingFactoryFixture, givenSharingTypeWhenRegisteringSharingThenBuilderLandsInItsOwnSlot) {
    ASSERT_EQ(0u, countRegisteredBuilders());

    SharingFactory::RegisterSharing<MockSharingBuilderFactory, FakeVaSharing> registerVa;

    EXPECT_EQ(1u, countRegisteredBuilders());
    EXPECT_NE(nullptr, SharingFactoryHelper::builders()[SharingType::VA_SHARING]);
}

TEST_F(SharingFactoryFixture, givenTwoSharingTypesWhenRegisteringSharingsThenTheyOccupyDistinctSlots) {
    SharingFactory::RegisterSharing<MockSharingBuilderFactory, FakeVaSharing> registerVa;
    SharingFactory::RegisterSharing<MockSharingBuilderFactory, FakeUnifiedSharing> registerUnified;

    auto builders = SharingFactoryHelper::builders();
    EXPECT_EQ(2u, countRegisteredBuilders());
    EXPECT_NE(nullptr, builders[SharingType::VA_SHARING]);
    EXPECT_NE(nullptr, builders[SharingType::UNIFIED_SHARING]);
    EXPECT_NE(builders[SharingType::VA_SHARING], builders[SharingType::UNIFIED_SHARING]);
}

TEST_F(SharingFactoryFixture, givenRegisteredSharingWhenBuildingThenItsContextBuilderIsUsed) {
    SharingFactory::RegisterSharing<MockSharingBuilderFactory, FakeVaSharing> registerVa;
    auto mock = static_cast<MockSharingBuilderFactory *>(SharingFactoryHelper::builders()[SharingType::VA_SHARING]);
    mock->processPropertiesResult = true;

    auto factory = SharingFactory::build();
    cl_context_properties propertyType = 0x4242;
    cl_context_properties propertyValue = 0;

    EXPECT_TRUE(factory->processProperties(propertyType, propertyValue));
    EXPECT_EQ(1u, mock->createContextBuilderCalled);
    EXPECT_EQ(1u, mock->processPropertiesCalled);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
