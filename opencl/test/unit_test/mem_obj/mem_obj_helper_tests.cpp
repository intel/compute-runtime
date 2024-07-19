/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemObjHelper, givenValidMemFlagsForSubBufferWhenFlagsAreCheckedThenTrueIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
                         CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(flags));
}

TEST(MemObjHelper, givenInvalidMemFlagsForSubBufferWhenFlagsAreCheckedThenTrueIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR;

    EXPECT_FALSE(MemObjHelper::checkMemFlagsForSubBuffer(flags));
}

TEST(MemObjHelper, givenClMemForceLinearStorageFlagWhenCheckForLinearStorageForceThenReturnProperValue) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_FALSE(memoryProperties.flags.forceLinearStorage);
}

TEST(MemObjHelper, givenValidPropertiesWhenValidatingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    UltClDeviceFactory deviceFactory{1, 0};
    auto pDevice = &deviceFactory.rootDevices[0]->getDevice();
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    MockContext context{deviceFactory.rootDevices[0]};

    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_NO_ACCESS_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_NO_ACCESS_INTEL;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_NO_ACCESS_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_NO_ACCESS_INTEL;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = 0;
    flagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flagsIntel = CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = 0;
    flagsIntel = CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = 0;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));
}

struct Image1dWithAccessFlagsUnrestricted : public Image1dDefaults {
    enum { flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL };
};

TEST(MemObjHelper, givenParentMemObjAndHostPtrFlagsWhenValidatingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    UltClDeviceFactory deviceFactory{1, 0};
    auto pDevice = &deviceFactory.rootDevices[0]->getDevice();
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);

    MockContext context{deviceFactory.rootDevices[0]};
    auto image = clUniquePtr(Image1dHelper<>::create(&context));
    auto imageWithAccessFlagsUnrestricted = clUniquePtr(ImageHelper<Image1dWithAccessFlagsUnrestricted>::create(&context));

    cl_mem_flags hostPtrFlags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_ALLOC_HOST_PTR, CL_MEM_COPY_HOST_PTR};

    for (auto hostPtrFlag : hostPtrFlags) {
        flags = hostPtrFlag;
        memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
        flags = hostPtrFlag;
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, image.get(), context));
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, imageWithAccessFlagsUnrestricted.get(), context));

        flags |= CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
        memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
        flags |= CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, image.get(), context));
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, imageWithAccessFlagsUnrestricted.get(), context));
    }
}

TEST(MemObjHelper, givenContextWithMultipleRootDevicesWhenIsSuitableForCompressionIsCalledThenFalseIsReturned) {
    MockDefaultContext context;

    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context.pRootDevice0->getDevice());
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelper, givenCompressionEnabledButNotPreferredWhenCompressionHintIsPassedThenCompressionIsUsed) {
    cl_mem_flags_intel flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL;
    cl_mem_flags flags = CL_MEM_COMPRESSED_HINT_INTEL;
    MockContext context;
    MemoryProperties memoryProperties =
        ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, false));
    flags = CL_MEM_COMPRESSED_HINT_INTEL;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, false));
    flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL;
    flags = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, false));
}

TEST(MemObjHelper, givenCompressionEnabledAndPreferredWhenCompressionHintIsPassedThenCompressionIsUsed) {
    cl_mem_flags_intel flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL;
    cl_mem_flags flags = CL_MEM_COMPRESSED_HINT_INTEL;
    MockContext context;
    MemoryProperties memoryProperties =
        ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
    flags = CL_MEM_COMPRESSED_HINT_INTEL;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
    flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL;
    flags = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelper, givenCompressionWhenCL_MEM_COMPRESSEDIsNotSetThenFalseReturned) {
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_flags flags = 0;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, false));
}

TEST(MemObjHelper, givenCompressionWhenCL_MEM_COMPRESSEDThenTrueIsReturned) {
    cl_mem_flags_intel flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL;
    cl_mem_flags flags = CL_MEM_COMPRESSED_HINT_INTEL;
    MockContext context;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelperMultiTile, givenValidExtraPropertiesWhenValidatingExtraPropertiesThenTrueIsReturned) {
    UltClDeviceFactory deviceFactory{1, 4};
    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.subDevices[0], deviceFactory.subDevices[1],
                              deviceFactory.subDevices[2], deviceFactory.subDevices[3]};
    MockContext context(ClDeviceVector{devices, 5});

    auto pDevice = &deviceFactory.rootDevices[0]->getDevice();

    cl_mem_flags flags = CL_MEM_COMPRESSED_HINT_INTEL;
    cl_mem_flags_intel flagsIntel = 0;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));

    flags = CL_MEM_UNCOMPRESSED_HINT_INTEL;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));

    flags = 0;
    flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));

    flags = 0;
    flagsIntel = CL_MEM_UNCOMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
}

TEST(MemObjHelperMultiTile, givenOneSubDeviceSelectedWhenParsingMemoryPropertiesThenTrueIsReturnedForValidContexts) {
    UltClDeviceFactory deviceFactory{1, 4};

    cl_device_id rootDeviceId = deviceFactory.rootDevices[0];
    MockContext rootContext(ClDeviceVector{&rootDeviceId, 1});

    cl_device_id tile0Id = deviceFactory.subDevices[0];
    MockContext tile0Context(ClDeviceVector{&tile0Id, 1});

    cl_device_id tile1Id = deviceFactory.subDevices[1];
    MockContext tile1Context(ClDeviceVector{&tile1Id, 1});

    cl_device_id allDevices[] = {deviceFactory.rootDevices[0], deviceFactory.subDevices[0], deviceFactory.subDevices[1],
                                 deviceFactory.subDevices[2], deviceFactory.subDevices[3]};
    MockContext multiTileContext(ClDeviceVector{allDevices, 5});

    EXPECT_EQ(deviceFactory.rootDevices[0]->getDeviceBitfield(), multiTileContext.getDevice(0)->getDeviceBitfield());

    auto parseMemoryProperties = [](ClDevice *pClDevice, Context &context) -> bool {
        cl_mem_flags flags = 0;
        cl_mem_flags_intel flagsIntel = 0;
        cl_mem_alloc_flags_intel allocFlagsIntel = 0;
        MemoryProperties memoryProperties{0};
        auto deviceIdProperty = reinterpret_cast<cl_mem_properties_intel>(static_cast<cl_device_id>(pClDevice));
        cl_mem_properties_intel properties[] = {CL_MEM_DEVICE_ID_INTEL, deviceIdProperty, 0};
        return ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocFlagsIntel,
                                                               ClMemoryPropertiesHelper::ObjType::buffer, context);
    };

    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[0], multiTileContext));
    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[0], tile0Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[0], tile1Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[0], rootContext));

    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[1], multiTileContext));
    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[1], tile1Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[1], tile0Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[1], rootContext));
}
TEST(MemObjHelperMultiTile, givenOneSubDeviceSelectedWithDeprecatedFlagWhenParsingMemoryPropertiesThenTrueIsReturnedForValidContexts) {
    UltClDeviceFactory deviceFactory{1, 4};

    cl_device_id rootDeviceId = deviceFactory.rootDevices[0];
    MockContext rootContext(ClDeviceVector{&rootDeviceId, 1});

    cl_device_id tile0Id = deviceFactory.subDevices[0];
    MockContext tile0Context(ClDeviceVector{&tile0Id, 1});

    cl_device_id tile1Id = deviceFactory.subDevices[1];
    MockContext tile1Context(ClDeviceVector{&tile1Id, 1});

    cl_device_id allDevices[] = {deviceFactory.rootDevices[0], deviceFactory.subDevices[0], deviceFactory.subDevices[1],
                                 deviceFactory.subDevices[2], deviceFactory.subDevices[3]};
    MockContext multiTileContext(ClDeviceVector{allDevices, 5});

    EXPECT_EQ(deviceFactory.rootDevices[0]->getDeviceBitfield(), multiTileContext.getDevice(0)->getDeviceBitfield());

    auto parseMemoryProperties = [](ClDevice *pClDevice, Context &context) -> bool {
        cl_mem_flags flags = 0;
        cl_mem_flags_intel flagsIntel = 0;
        cl_mem_alloc_flags_intel allocFlagsIntel = 0;
        MemoryProperties memoryProperties{0};
        auto deviceIdProperty = reinterpret_cast<cl_mem_properties_intel>(static_cast<cl_device_id>(pClDevice));
        cl_mem_properties_intel properties[] = {CL_MEM_DEVICE_ID_INTEL_DEPRECATED, deviceIdProperty, 0};
        return ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocFlagsIntel,
                                                               ClMemoryPropertiesHelper::ObjType::buffer, context);
    };

    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[0], multiTileContext));
    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[0], tile0Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[0], tile1Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[0], rootContext));

    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[1], multiTileContext));
    EXPECT_TRUE(parseMemoryProperties(deviceFactory.subDevices[1], tile1Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[1], tile0Context));
    EXPECT_FALSE(parseMemoryProperties(deviceFactory.subDevices[1], rootContext));
}

TEST(MemObjHelper, givenInvalidFlagsWhenValidatingExtraPropertiesThenFalseIsReturned) {
    MemoryProperties memoryProperties;
    cl_mem_flags flags = CL_MEM_COMPRESSED_HINT_INTEL | CL_MEM_UNCOMPRESSED_HINT_INTEL;
    cl_mem_flags_intel flagsIntel = 0;
    MockContext context;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));

    flags = 0;
    flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL | CL_MEM_UNCOMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));

    flags = CL_MEM_COMPRESSED_HINT_INTEL;
    flagsIntel = CL_MEM_UNCOMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));

    flags = CL_MEM_UNCOMPRESSED_HINT_INTEL;
    flagsIntel = CL_MEM_COMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &context.getDevice(0)->getDevice());
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
}

TEST(MemObjHelper, givenMultipleSubDevicesWhenDefaultContextIsUsedThenResourcesAreNotSuitableForCompression) {
    DebugManagerStateRestore debugRestore;
    debugManager.flags.CreateMultipleSubDevices.set(4u);
    initPlatform();
    MockContext context(platform()->getClDevice(0));
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_ONLY, 0u, 0, &context.getDevice(0)->getDevice());

    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
    memoryProperties.flags.hostNoAccess = true;
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelper, givenCompressionEnabledAndPreferredWhenContextRequiresResolveThenResourceNotSuitableForCompression) {
    MemoryProperties memoryProperties;
    MockContext context;

    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    context.resolvesRequiredInKernels = true;

    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelper, givenCompressionEnabledAndPreferredWhenContextNotRequiresResolveThenResourceSuitableForCompression) {
    MemoryProperties memoryProperties;
    MockContext context;

    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    context.resolvesRequiredInKernels = false;

    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelper, givenCompressionEnabledAndPreferredWhenContextNotRequiresResolveAndForceHintDisableCompressionThenResourceNotSuitableForCompression) {
    DebugManagerStateRestore restore;
    debugManager.flags.ToggleHintKernelDisableCompression.set(0);

    MemoryProperties memoryProperties;
    MockContext context;

    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    context.resolvesRequiredInKernels = false;

    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelper, givenCompressionEnabledAndPreferredWhenContextRequiresResolveAndForceHintEnableCompressionThenResourceSuitableForCompression) {
    DebugManagerStateRestore restore;
    debugManager.flags.ToggleHintKernelDisableCompression.set(1);

    MemoryProperties memoryProperties;
    MockContext context;

    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    context.resolvesRequiredInKernels = true;

    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, memoryProperties, context, true));
}

TEST(MemObjHelper, givenDifferentCapabilityAndDebugFlagValuesWhenCheckingBufferCompressionSupportThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestore;
    VariableBackup<bool> renderCompressedBuffersCapability{&defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers};
    int32_t enableMultiTileCompressionValues[] = {-1, 0, 1};

    for (auto ftrRenderCompressedBuffers : ::testing::Bool()) {
        renderCompressedBuffersCapability = ftrRenderCompressedBuffers;
        for (auto enableMultiTileCompressionValue : enableMultiTileCompressionValues) {
            debugManager.flags.EnableMultiTileCompression.set(enableMultiTileCompressionValue);

            MockSpecializedContext context;
            auto &device = context.getDevice(0)->getDevice();
            MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &device);

            bool compressionEnabled = MemObjHelper::isSuitableForCompression(GfxCoreHelper::compressedBuffersSupported(*defaultHwInfo), memoryProperties, context, true);

            MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(
                memoryProperties, compressionEnabled, false);

            bool expectBufferCompressed = ftrRenderCompressedBuffers && (enableMultiTileCompressionValue == 1);
            if (expectBufferCompressed) {
                EXPECT_TRUE(compressionEnabled);
            } else {
                EXPECT_FALSE(compressionEnabled);
            }
        }
    }
}

TEST(MemObjHelper, givenDifferentValuesWhenCheckingBufferCompressionSupportThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestore;
    VariableBackup<bool> renderCompressedBuffersCapability{&defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers, true};
    VariableBackup<unsigned short> hardwareStepping{&defaultHwInfo->platform.usRevId};
    debugManager.flags.EnableMultiTileCompression.set(1);

    uint32_t numsSubDevices[] = {0, 2};
    cl_mem_flags flagsValues[] = {0, CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS, CL_MEM_COMPRESSED_HINT_INTEL,
                                  CL_MEM_UNCOMPRESSED_HINT_INTEL};
    cl_mem_flags_intel flagsIntelValues[] = {0, CL_MEM_COMPRESSED_HINT_INTEL, CL_MEM_UNCOMPRESSED_HINT_INTEL};
    uint32_t contextTypes[] = {ContextType::CONTEXT_TYPE_DEFAULT, ContextType::CONTEXT_TYPE_SPECIALIZED,
                               ContextType::CONTEXT_TYPE_UNRESTRICTIVE};

    for (auto numSubDevices : numsSubDevices) {
        UltClDeviceFactory clDeviceFactory{1, numSubDevices};

        for (auto contextType : contextTypes) {
            if ((numSubDevices == 0) && (contextType != ContextType::CONTEXT_TYPE_DEFAULT)) {
                continue;
            }

            ClDeviceVector contextDevices;
            if (contextType != ContextType::CONTEXT_TYPE_SPECIALIZED) {
                contextDevices.push_back(clDeviceFactory.rootDevices[0]);
            }
            if (contextType != ContextType::CONTEXT_TYPE_DEFAULT) {
                contextDevices.push_back(clDeviceFactory.subDevices[0]);
                contextDevices.push_back(clDeviceFactory.subDevices[1]);
            }
            MockContext context{contextDevices};

            for (auto flags : flagsValues) {
                for (auto flagsIntel : flagsIntelValues) {

                    auto &device = context.getDevice(0)->getDevice();
                    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel,
                                                                                                         0, &device);

                    bool compressionEnabled = MemObjHelper::isSuitableForCompression(GfxCoreHelper::compressedBuffersSupported(*defaultHwInfo), memoryProperties, context, true);
                    MockPublicAccessBuffer::getGraphicsAllocationTypeAndCompressionPreference(
                        memoryProperties, compressionEnabled, false);

                    bool isCompressionDisabled = isValueSet(flags, CL_MEM_UNCOMPRESSED_HINT_INTEL) ||
                                                 isValueSet(flagsIntel, CL_MEM_UNCOMPRESSED_HINT_INTEL);
                    bool expectBufferCompressed = !isCompressionDisabled;

                    bool isMultiTile = (numSubDevices > 1);
                    if (expectBufferCompressed && isMultiTile) {
                        bool isBufferReadOnly = isValueSet(flags, CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS);
                        expectBufferCompressed = (contextType == ContextType::CONTEXT_TYPE_SPECIALIZED) || isBufferReadOnly;
                    }

                    if (expectBufferCompressed) {
                        EXPECT_TRUE(compressionEnabled);
                    } else {
                        EXPECT_FALSE(compressionEnabled);
                    }
                }
            }
        }
    }
}
