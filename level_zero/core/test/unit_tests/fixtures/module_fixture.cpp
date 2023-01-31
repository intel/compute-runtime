/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

ModuleImmutableDataFixture::MockImmutableMemoryManager::MockImmutableMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}

ModuleImmutableDataFixture::MockImmutableData::MockImmutableData(uint32_t perHwThreadPrivateMemorySize) : MockImmutableData(perHwThreadPrivateMemorySize, 0, 0) {}
ModuleImmutableDataFixture::MockImmutableData::MockImmutableData(uint32_t perHwThreadPrivateMemorySize, uint32_t perThreadScratchSize, uint32_t perThreaddPrivateScratchSize) {
    mockKernelDescriptor = new NEO::KernelDescriptor;
    mockKernelDescriptor->kernelAttributes.perHwThreadPrivateMemorySize = perHwThreadPrivateMemorySize;
    mockKernelDescriptor->kernelAttributes.perThreadScratchSize[0] = perThreadScratchSize;
    mockKernelDescriptor->kernelAttributes.perThreadScratchSize[1] = perThreaddPrivateScratchSize;
    kernelDescriptor = mockKernelDescriptor;

    mockKernelInfo = new NEO::KernelInfo;
    mockKernelInfo->heapInfo.pKernelHeap = kernelHeap;
    mockKernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    kernelInfo = mockKernelInfo;

    auto ptr = reinterpret_cast<void *>(0x1234);
    isaGraphicsAllocation.reset(new NEO::MockGraphicsAllocation(0,
                                                                NEO::AllocationType::KERNEL_ISA,
                                                                ptr,
                                                                0x1000,
                                                                0u,
                                                                MemoryPool::System4KBPages,
                                                                MemoryManager::maxOsContextCount,
                                                                castToUint64(ptr)));
    kernelInfo->kernelAllocation = isaGraphicsAllocation.get();
}

ModuleImmutableDataFixture::MockImmutableData::~MockImmutableData() {
    delete mockKernelInfo;
    delete mockKernelDescriptor;
}
void ModuleImmutableDataFixture::MockImmutableData::resizeExplicitArgs(size_t size) {
    kernelDescriptor->payloadMappings.explicitArgs.resize(size);
}

ModuleImmutableDataFixture::MockModule::MockModule(L0::Device *device,
                                                   L0::ModuleBuildLog *moduleBuildLog,
                                                   L0::ModuleType type,
                                                   uint32_t perHwThreadPrivateMemorySize,
                                                   MockImmutableData *inMockKernelImmData) : ModuleImp(device, moduleBuildLog, type), mockKernelImmData(inMockKernelImmData) {
    this->mockKernelImmData->setDevice(device);
    this->translationUnit.reset(new MockModuleTranslationUnit(this->device));
}

void ModuleImmutableDataFixture::MockModule::checkIfPrivateMemoryPerDispatchIsNeeded() {
    const_cast<KernelDescriptor &>(kernelImmDatas[0]->getDescriptor()).kernelAttributes.perHwThreadPrivateMemorySize = mockKernelImmData->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize;
    ModuleImp::checkIfPrivateMemoryPerDispatchIsNeeded();
}

void ModuleImmutableDataFixture::MockKernel::setCrossThreadData(uint32_t dataSize) {
    crossThreadData.reset(new uint8_t[dataSize]);
    crossThreadDataSize = dataSize;
    memset(crossThreadData.get(), 0x00, crossThreadDataSize);
}

void ModuleImmutableDataFixture::setUp() {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0u);
    memoryManager = new MockImmutableMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
}

void ModuleImmutableDataFixture::createModuleFromMockBinary(uint32_t perHwThreadPrivateMemorySize, bool isInternal, MockImmutableData *mockKernelImmData, std::initializer_list<ZebinTestData::appendElfAdditionalSection> additionalSections) {
    zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;

    module = std::make_unique<MockModule>(device,
                                          moduleBuildLog,
                                          ModuleType::User,
                                          perHwThreadPrivateMemorySize,
                                          mockKernelImmData);

    module->type = isInternal ? ModuleType::Builtin : ModuleType::User;
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

void ModuleImmutableDataFixture::createKernel(MockKernel *kernel) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    kernel->initialize(&desc);
}

void ModuleImmutableDataFixture::tearDown() {
    module.reset(nullptr);
    DeviceFixture::tearDown();
}

L0::Module *ModuleFixture::ProxyModuleImp::create(L0::Device *device, const ze_module_desc_t *desc,
                                                  ModuleBuildLog *moduleBuildLog, ModuleType type, ze_result_t *result) {
    auto module = new ProxyModuleImp(device, moduleBuildLog, type);

    *result = module->initialize(desc, device->getNEODevice());
    if (*result != ZE_RESULT_SUCCESS) {
        module->destroy();
        return nullptr;
    }

    return module;
}

void ModuleFixture::setUp() {

    DeviceFixture::setUp();
    createModuleFromMockBinary();
}

void ModuleFixture::createModuleFromMockBinary(ModuleType type) {
    zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;
    ze_result_t result = ZE_RESULT_SUCCESS;
    module.reset(ProxyModuleImp::create(device, &moduleDesc, moduleBuildLog, type, &result));
}

void ModuleFixture::createKernel() {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
    kernel->module = module.get();
    kernel->initialize(&desc);
}

std::unique_ptr<WhiteBox<::L0::Kernel>> ModuleFixture::createKernelWithName(std::string name) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = name.c_str();

    auto kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
    kernel->module = module.get();
    kernel->initialize(&desc);
    return kernel;
}

void ModuleFixture::tearDown() {
    kernel.reset(nullptr);
    module.reset(nullptr);
    DeviceFixture::tearDown();
}

void MultiDeviceModuleFixture::setUp() {
    MultiDeviceFixture::setUp();
    modules.resize(numRootDevices);
}

void MultiDeviceModuleFixture::createModuleFromMockBinary(uint32_t rootDeviceIndex) {
    auto device = driverHandle->devices[rootDeviceIndex];
    zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;
    ze_result_t result = ZE_RESULT_SUCCESS;

    modules[rootDeviceIndex].reset(Module::create(device,
                                                  &moduleDesc,
                                                  moduleBuildLog, ModuleType::User, &result));
}

void MultiDeviceModuleFixture::createKernel(uint32_t rootDeviceIndex) {
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
    kernel->module = modules[rootDeviceIndex].get();
    kernel->initialize(&desc);
}

void MultiDeviceModuleFixture::tearDown() {
    kernel.reset(nullptr);
    for (auto &module : modules) {
        module.reset(nullptr);
    }
    MultiDeviceFixture::tearDown();
}
ModuleWithZebinFixture::MockImmutableData::MockImmutableData(L0::Device *device) {

    auto mockKernelDescriptor = new NEO::KernelDescriptor;
    mockKernelDescriptor->kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;
    kernelDescriptor = mockKernelDescriptor;
    this->device = device;
    auto ptr = reinterpret_cast<void *>(0x1234);
    isaGraphicsAllocation.reset(new NEO::MockGraphicsAllocation(0,
                                                                NEO::AllocationType::KERNEL_ISA,
                                                                ptr,
                                                                0x1000,
                                                                0u,
                                                                MemoryPool::System4KBPages,
                                                                MemoryManager::maxOsContextCount,
                                                                castToUint64(ptr)));
}

ModuleWithZebinFixture::MockImmutableData::~MockImmutableData() {
    delete kernelDescriptor;
}

ModuleWithZebinFixture::MockModuleWithZebin::MockModuleWithZebin(L0::Device *device) : ModuleImp(device, nullptr, ModuleType::User) {
    isZebinBinary = true;
}
void ModuleWithZebinFixture::MockModuleWithZebin::addSegments() {
    kernelImmDatas.push_back(std::make_unique<MockImmutableData>(device));
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto canonizedGpuAddress = castToUint64(ptr);
    translationUnit->globalVarBuffer = new NEO::MockGraphicsAllocation(0,
                                                                       NEO::AllocationType::GLOBAL_SURFACE,
                                                                       ptr,
                                                                       0x1000,
                                                                       0u,
                                                                       MemoryPool::System4KBPages,
                                                                       MemoryManager::maxOsContextCount,
                                                                       canonizedGpuAddress);
    translationUnit->globalConstBuffer = new NEO::MockGraphicsAllocation(0,
                                                                         NEO::AllocationType::GLOBAL_SURFACE,
                                                                         ptr,
                                                                         0x1000,
                                                                         0u,
                                                                         MemoryPool::System4KBPages,
                                                                         MemoryManager::maxOsContextCount,
                                                                         canonizedGpuAddress);

    translationUnit->programInfo.globalStrings.initData = &strings;
    translationUnit->programInfo.globalStrings.size = sizeof(strings);
}

void ModuleWithZebinFixture::MockModuleWithZebin::addEmptyZebin() {
    auto zebin = ZebinTestData::ValidEmptyProgram<>();

    translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(translationUnit->unpackedDeviceBinary.get(), translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());
}

void ModuleWithZebinFixture::setUp() {

    DeviceFixture::setUp();
    module = std::make_unique<MockModuleWithZebin>(device);
}

void ModuleWithZebinFixture::tearDown() {
    module.reset(nullptr);
    DeviceFixture::tearDown();
}

void ImportHostPointerModuleFixture::setUp() {
    DebugManager.flags.EnableHostPointerImport.set(1);
    ModuleFixture::setUp();

    hostPointer = driverHandle->getMemoryManager()->allocateSystemMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
}

void ImportHostPointerModuleFixture::tearDown() {
    driverHandle->getMemoryManager()->freeSystemMemory(hostPointer);
    ModuleFixture::tearDown();
}

MultiTileModuleFixture::MultiTileModuleFixture() : backup({&NEO::ImplicitScaling::apiSupport, true}) {}
void MultiTileModuleFixture::setUp() {
    DebugManager.flags.EnableImplicitScaling.set(1);
    MultiDeviceFixture::numRootDevices = 1u;
    MultiDeviceFixture::numSubDevices = 2u;

    MultiDeviceModuleFixture::setUp();
    createModuleFromMockBinary(0);

    device = driverHandle->devices[0];
}

void MultiTileModuleFixture::tearDown() {
    MultiDeviceModuleFixture::tearDown();
}
} // namespace ult
} // namespace L0
