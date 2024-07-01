/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/debug_mock_drm_xe.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "debug_xe_includes.h"

using namespace NEO;

TEST(IoctlHelperXeTest, whenCallingDebuggerOpenIoctlThenProperValueIsReturned) {
    int ret;
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto mockXeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    drm.reset();
    drm_xe_eudebug_connect test = {};

    ret = mockXeIoctlHelper->ioctl(DrmIoctl::debuggerOpen, &test);
    EXPECT_EQ(ret, drm.debuggerOpenRetval);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetIoctForDebuggerThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    auto verifyIoctlRequestValue = [&xeIoctlHelper](auto value, DrmIoctl drmIoctl) {
        EXPECT_EQ(xeIoctlHelper->getIoctlRequestValue(drmIoctl), static_cast<unsigned int>(value));
    };
    auto verifyIoctlString = [&xeIoctlHelper](DrmIoctl drmIoctl, const char *string) {
        EXPECT_STREQ(string, xeIoctlHelper->getIoctlString(drmIoctl).c_str());
    };

    verifyIoctlString(DrmIoctl::debuggerOpen, "DRM_IOCTL_XE_EUDEBUG_CONNECT");

    verifyIoctlRequestValue(DRM_IOCTL_XE_EUDEBUG_CONNECT, DrmIoctl::debuggerOpen);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingaddDebugMetadataThenDataIsAdded) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    uint64_t temp = 0;
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::moduleHeapDebugArea, &temp, 8000u);
    ASSERT_EQ(1u, xeIoctlHelper->debugMetadata.size());
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::contextSaveArea, &temp, 8000u);
    ASSERT_EQ(2u, xeIoctlHelper->debugMetadata.size());
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::sbaTrackingBuffer, &temp, 8000u);
    ASSERT_EQ(3u, xeIoctlHelper->debugMetadata.size());
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::isa, &temp, 8000u); // ISA should be ignored
    ASSERT_EQ(3u, xeIoctlHelper->debugMetadata.size());
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingVmCreateThenDebugMetadadaIsAttached) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    uint64_t temp = 0;
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::moduleHeapDebugArea, &temp, 8000u);
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::contextSaveArea, &temp, 8000u);
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::sbaTrackingBuffer, &temp, 8000u);
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::isa, &temp, 8000u); // ISA should be ignored
    xeIoctlHelper->addDebugMetadataCookie(123u);

    GemVmControl test = {};
    xeIoctlHelper->ioctl(DrmIoctl::gemVmCreate, &test);

    ASSERT_EQ(4u, drm.vmCreateMetadata.size());
    ASSERT_EQ(drm.vmCreateMetadata[0].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_MODULE_AREA));
    ASSERT_EQ(drm.vmCreateMetadata[0].offset, reinterpret_cast<unsigned long long>(&temp));
    ASSERT_EQ(drm.vmCreateMetadata[0].len, 8000ul);

    ASSERT_EQ(drm.vmCreateMetadata[1].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_SIP_AREA));
    ASSERT_EQ(drm.vmCreateMetadata[1].offset, reinterpret_cast<unsigned long long>(&temp));
    ASSERT_EQ(drm.vmCreateMetadata[1].len, 8000ul);

    ASSERT_EQ(drm.vmCreateMetadata[2].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_SBA_AREA));
    ASSERT_EQ(drm.vmCreateMetadata[2].offset, reinterpret_cast<unsigned long long>(&temp));
    ASSERT_EQ(drm.vmCreateMetadata[2].len, 8000ul);

    ASSERT_EQ(drm.vmCreateMetadata[3].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_COOKIE));
    ASSERT_EQ(drm.vmCreateMetadata[3].offset, 123ul);
    ASSERT_EQ(drm.vmCreateMetadata[3].len, 0ul);
    ASSERT_EQ(drm.vmCreateMetadata[3].base.next_extension, 0ul);
}

TEST(IoctlHelperXeTest, givenFreeDebugMetadataWhenVmCreateHasMultipleExtTypesThenOnlyDebugMetadataIsDeleted) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    drm_xe_ext_vm_set_debug_metadata *node1 = new drm_xe_ext_vm_set_debug_metadata();
    drm_xe_ext_vm_set_debug_metadata *node2 = new drm_xe_ext_vm_set_debug_metadata();
    drm_xe_ext_vm_set_debug_metadata *node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node2->base.name = 0x1234ul;
    node3->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    drm_xe_ext_vm_set_debug_metadata *newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, node2);
    ASSERT_EQ(newRoot->base.next_extension, 0ul);
    delete node2;

    node1 = new drm_xe_ext_vm_set_debug_metadata();
    node2 = new drm_xe_ext_vm_set_debug_metadata();
    node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node2->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node3->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, nullptr);

    node1 = new drm_xe_ext_vm_set_debug_metadata();
    node2 = new drm_xe_ext_vm_set_debug_metadata();
    node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = 0x1234;
    node2->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node3->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, node1);
    ASSERT_EQ(newRoot->base.next_extension, 0ul);
    delete node1;

    node1 = new drm_xe_ext_vm_set_debug_metadata();
    node2 = new drm_xe_ext_vm_set_debug_metadata();
    node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = 0x1234;
    node2->base.name = 0x1234;
    node3->base.name = 0x1234;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, node1);
    ASSERT_EQ(newRoot->base.next_extension, reinterpret_cast<unsigned long long>(node2));
    delete node1;
    delete node2;
    delete node3;
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetRunaloneExtPropertyThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    EXPECT_EQ(xeIoctlHelper->getRunaloneExtProperty(), DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG);
}

using IoctlHelperXeTestFixture = ::testing::Test;
HWTEST_F(IoctlHelperXeTestFixture, GivenRunaloneModeRequiredReturnFalseWhenCreateDrmContextThenRunAloneContextIsNotRequested) {
    DebugManagerStateRestore restorer;
    struct MockGfxCoreHelperHw : NEO::GfxCoreHelperHw<FamilyType> {
        bool isRunaloneModeRequired(DebuggingMode debuggingMode) const override {
            return false;
        }
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw>(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.ioctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm.getIoctlHelper());

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm.engineInfo = std::move(engineInfo);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));
    uint16_t deviceIndex = 1;
    xeIoctlHelper->createDrmContext(drm, osContext, 0, deviceIndex, false);

    auto ext = drm.receivedContextCreateSetParam;
    EXPECT_NE(ext.property, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG));
}

HWTEST_F(IoctlHelperXeTestFixture, givenDeviceIndexWhenCreatingContextThenSetCorrectGtId) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];

    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.ioctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm.getIoctlHelper());
    xeIoctlHelper->initialize();

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm.engineInfo = std::move(engineInfo);

    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
    engineDescriptor.isEngineInstanced = true;
    OsContextLinux osContext(drm, 0, 0u, engineDescriptor);

    uint16_t tileId = 1u;
    uint16_t expectedGtId = xeIoctlHelper->tileIdToGtId[tileId];

    xeIoctlHelper->createDrmContext(drm, osContext, 0, tileId, false);

    EXPECT_EQ(1u, drm.execQueueCreateParams.num_placements);
    ASSERT_EQ(1u, drm.execQueueEngineInstances.size());

    EXPECT_EQ(expectedGtId, drm.execQueueEngineInstances[0].gt_id);
}

HWTEST_F(IoctlHelperXeTestFixture, GivenRunaloneModeRequiredReturnTrueWhenCreateDrmContextThenRunAloneContextIsRequested) {
    DebugManagerStateRestore restorer;
    struct MockGfxCoreHelperHw : NEO::GfxCoreHelperHw<FamilyType> {
        bool isRunaloneModeRequired(DebuggingMode debuggingMode) const override {
            return true;
        }
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw>(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.ioctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm.getIoctlHelper());

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm.engineInfo = std::move(engineInfo);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));
    xeIoctlHelper->createDrmContext(drm, osContext, 0, 1, false);

    auto ext = drm.receivedContextCreateSetParam;
    EXPECT_EQ(ext.base.name, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY));
    EXPECT_EQ(ext.base.next_extension, 0ULL);
    EXPECT_EQ(ext.property, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG));
    EXPECT_EQ(ext.value, 1ULL);
}

HWTEST_F(IoctlHelperXeTestFixture, GivenContextCreatedForCopyEngineWhenCreateDrmContextThenRunAloneContextIsNotRequested) {
    DebugManagerStateRestore restorer;
    struct MockGfxCoreHelperHw : NEO::GfxCoreHelperHw<FamilyType> {
        bool isRunaloneModeRequired(DebuggingMode debuggingMode) const override {
            return true;
        }
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw>(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.ioctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm.getIoctlHelper());

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm.engineInfo = std::move(engineInfo);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS1, EngineUsage::regular}));
    xeIoctlHelper->createDrmContext(drm, osContext, 0, 0, false);

    auto ext = drm.receivedContextCreateSetParam;
    EXPECT_NE(ext.property, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG));
}

TEST(IoctlHelperXeTest, GivenXeDriverThenDebugAttachReturnsTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    EXPECT_TRUE(xeIoctlHelper->isDebugAttachAvailable());
}

TEST(IoctlHelperXeTest, givenXeEnableEuDebugThenReturnCorrectValue) {

    VariableBackup<size_t> mockFreadReturnBackup(&IoFunctions::mockFreadReturn, 1);
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(IoFunctions::mockFreadReturn);
    VariableBackup<char *> mockFreadBufferBackup(&IoFunctions::mockFreadBuffer, buffer.get());

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    buffer[0] = '1';
    int enableEuDebug = xeIoctlHelper->getEuDebugSysFsEnable();
    EXPECT_EQ(1, enableEuDebug);

    buffer[0] = '0';
    enableEuDebug = xeIoctlHelper->getEuDebugSysFsEnable();
    EXPECT_EQ(0, enableEuDebug);
}

TEST(IoctlHelperXeTest, givenXeEnableEuDebugWithInvalidPathThenReturnCorrectValue) {
    VariableBackup<size_t> mockFreadReturnBackup(&IoFunctions::mockFreadReturn, 1);
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(IoFunctions::mockFreadReturn);
    VariableBackup<char *> mockFreadBufferBackup(&IoFunctions::mockFreadBuffer, buffer.get());

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    buffer[0] = '1';
    VariableBackup<FILE *> mockFopenReturnBackup(&IoFunctions::mockFopenReturned, nullptr);
    int enableEuDebug = xeIoctlHelper->getEuDebugSysFsEnable();

    EXPECT_EQ(0, enableEuDebug);
}

TEST(IoctlHelperXeTest, givenXeRegisterResourceThenCorrectIoctlCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    constexpr size_t bufferSize = 20;
    uint8_t buffer[bufferSize];
    auto id = xeIoctlHelper->registerResource(DrmResourceClass::elf, buffer, bufferSize);
    EXPECT_EQ(drm.metadataID, id);
    EXPECT_EQ(drm.metadataAddr, buffer);
    EXPECT_EQ(drm.metadataSize, bufferSize);
    EXPECT_EQ(drm.metadataType, static_cast<uint64_t>(DRM_XE_DEBUG_METADATA_ELF_BINARY));

    drm.metadataID = 0;
    drm.metadataAddr = nullptr;
    drm.metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::l0ZebinModule, buffer, bufferSize);
    EXPECT_EQ(drm.metadataID, id);
    EXPECT_EQ(drm.metadataAddr, buffer);
    EXPECT_EQ(drm.metadataSize, bufferSize);
    EXPECT_EQ(drm.metadataType, static_cast<uint64_t>(DRM_XE_DEBUG_METADATA_PROGRAM_MODULE));

    drm.metadataID = 0;
    drm.metadataAddr = nullptr;
    drm.metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::contextSaveArea, buffer, bufferSize);
    EXPECT_EQ(drm.metadataID, id);
    EXPECT_EQ(drm.metadataAddr, buffer);
    EXPECT_EQ(drm.metadataSize, bufferSize);
    EXPECT_EQ(drm.metadataType, static_cast<uint64_t>(WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA));

    drm.metadataID = 0;
    drm.metadataAddr = nullptr;
    drm.metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::sbaTrackingBuffer, buffer, bufferSize);
    EXPECT_EQ(drm.metadataID, id);
    EXPECT_EQ(drm.metadataAddr, buffer);
    EXPECT_EQ(drm.metadataSize, bufferSize);
    EXPECT_EQ(drm.metadataType, static_cast<uint64_t>(WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA));

    drm.metadataID = 0;
    drm.metadataAddr = nullptr;
    drm.metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::moduleHeapDebugArea, buffer, bufferSize);
    EXPECT_EQ(drm.metadataID, id);
    EXPECT_EQ(drm.metadataAddr, buffer);
    EXPECT_EQ(drm.metadataSize, bufferSize);
    EXPECT_EQ(drm.metadataType, static_cast<uint64_t>(WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA));
}

TEST(IoctlHelperXeTest, givenXeunregisterResourceThenCorrectIoctlCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    xeIoctlHelper->unregisterResource(0x1234);
    EXPECT_EQ(drm.metadataID, 0x1234u);
}

TEST(IoctlHelperXeTest, whenGettingVmBindExtFromHandlesThenProperStructsAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = xeIoctlHelper->prepareVmBindExt(bindExtHandles);
    auto vmBindExt = reinterpret_cast<drm_xe_vm_bind_op_ext_attach_debug *>(retVal.get());

    for (size_t i = 0; i < bindExtHandles.size(); i++) {

        EXPECT_EQ(bindExtHandles[i], vmBindExt[i].metadata_id);
        EXPECT_EQ(static_cast<uint32_t>(XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG), vmBindExt[i].base.name);
    }

    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[1]), vmBindExt[0].base.next_extension);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[2]), vmBindExt[1].base.next_extension);
}

TEST(IoctlHelperXeTest, givenResourceRegistrationEnabledWhenAllocationTypeShouldBeRegisteredThenBoHasBindExtHandleAdded) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    drm.ioctlHelper.reset(xeIoctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::contextSaveArea, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::sbaTrackingBuffer, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugModuleArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::moduleHeapDebugArea, drm.registeredClass);
    }

    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::bufferHostMemory, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
        EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
    }
}

TEST(IoctlHelperXeTest, givenResourceRegistrationEnabledWhenAllocationTypeShouldNotBeRegisteredThenNoBindHandleCreated) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    drm.registeredClass = DrmResourceClass::maxSize;
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    drm.ioctlHelper.reset(xeIoctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsaInternal, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
    }
    EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
}
