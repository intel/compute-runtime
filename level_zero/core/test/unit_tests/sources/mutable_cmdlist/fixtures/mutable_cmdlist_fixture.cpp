/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"

#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {

void MutableCommandListFixtureInit::setUp(bool createInOrder) {
    this->createInOrder = createInOrder;
    ModuleImmutableDataFixture::setUp();

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    this->engineGroupType = gfxCoreHelper.getEngineGroupType(neoDevice->getDefaultEngine().getEngineType(), neoDevice->getDefaultEngine().getEngineUsage(), device->getHwInfo());

    mutableCommandList = createMutableCmdList();

    mockKernelImmData2 = std::make_unique<MockImmutableData>(0u);
    mockKernelImmData2->kernelDescriptor->kernelAttributes.crossThreadDataSize = crossThreadInitSize;
    mockKernelImmData2->crossThreadDataSize = crossThreadInitSize;
    mockKernelImmData2->crossThreadDataTemplate.reset(new uint8_t[crossThreadInitSize]);

    mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0;
    mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.offset = 8;

    {
        std::initializer_list<ZebinTestData::AppendElfAdditionalSection> additionalSections = {};
        zebinData2 = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
        const auto &src = zebinData2->storage;

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
        moduleDesc.inputSize = src.size();

        ModuleBuildLog *moduleBuildLog = nullptr;

        module2 = std::make_unique<MockModule>(device,
                                               moduleBuildLog,
                                               ModuleType::user,
                                               0,
                                               mockKernelImmData2.get());

        module2->type = ModuleType::user;
        ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        result = module2->initialize(&moduleDesc, device->getNEODevice());
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    kernel2 = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module2.get());
    createKernel(kernel2.get());
    module2->mockKernelImmData->kernelDescriptor->kernelMetadata.kernelName = "test2";

    mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    mockKernelImmData->kernelDescriptor->kernelAttributes.crossThreadDataSize = crossThreadInitSize;
    mockKernelImmData->crossThreadDataSize = crossThreadInitSize;
    mockKernelImmData->crossThreadDataTemplate.reset(new uint8_t[crossThreadInitSize]);
    mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0;
    mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.offset = 8;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    createKernel(kernel.get());

    kernelHandle = kernelMutationGroup[0] = kernel->toHandle();
    kernel2Handle = kernelMutationGroup[1] = kernel2->toHandle();
}

void MutableCommandListFixtureInit::tearDown() {
    for (auto eventHandle : this->eventHandles) {
        zeEventDestroy(eventHandle);
    }
    if (this->eventPoolHandle) {
        zeEventPoolDestroy(this->eventPoolHandle);
    }
    for (auto externalStorage : this->externalStorages) {
        context->freeMem(externalStorage);
    }

    auto svmAllocsManager = this->device->getDriverHandle()->getSvmAllocsManager();
    for (auto usmAllocation : usmAllocations) {
        svmAllocsManager->freeSVMAlloc(usmAllocation);
    }
    mutableCommandList.reset(nullptr);

    kernel.reset(nullptr);
    kernel2.reset(nullptr);

    module.reset(nullptr);
    module2.reset(nullptr);

    mockKernelImmData.reset(nullptr);
    mockKernelImmData2.reset(nullptr);

    ModuleImmutableDataFixture::tearDown();
}

std::unique_ptr<MutableCommandList> MutableCommandListFixtureInit::createMutableCmdList() {
    ze_result_t returnValue;

    ze_command_list_flags_t flags = 0;
    if (this->createInOrder) {
        flags |= ZE_COMMAND_LIST_FLAG_IN_ORDER;
    }

    std::unique_ptr<MutableCommandList> mutableCommandListPtr(
        MutableCommandList::whiteboxCast(::L0::MCL::MutableCommandList::fromHandle(::L0::MCL::MutableCommandList::create(productFamily, this->device, this->engineGroupType, flags, returnValue, false))));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    return mutableCommandListPtr;
}

void *MutableCommandListFixtureInit::allocateUsm(size_t size) {
    auto svmAllocsManager = this->device->getDriverHandle()->getSvmAllocsManager();

    auto allocationProperties = NEO::SVMAllocsManager::SvmAllocationProperties{};
    auto usmPtr = svmAllocsManager->createSVMAlloc(size, allocationProperties, this->context->rootDeviceIndices, this->context->deviceBitfields);
    usmAllocations.push_back(usmPtr);
    return usmPtr;
}

NEO::GraphicsAllocation *MutableCommandListFixtureInit::getUsmAllocation(void *usm) {
    auto svmAllocsManager = this->device->getDriverHandle()->getSvmAllocsManager();
    auto allocData = svmAllocsManager->getSVMAlloc(usm);
    if (allocData != nullptr) {
        return allocData->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex());
    }
    return nullptr;
}

Event *MutableCommandListFixtureInit::createTestEvent(bool cbEvent, bool signalScope, bool timestamp, bool external) {
    Event *event = nullptr;
    if (cbEvent) {
        zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
        zex_counter_based_event_external_storage_properties_t externalStorageAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES};
        if (external) {
            void *externalStorage = nullptr;
            ze_device_mem_alloc_desc_t deviceDesc = {};
            context->allocDeviceMem(device->toHandle(), &deviceDesc, sizeof(uint64_t), 4096u, &externalStorage);
            this->externalEventDeviceAddress = reinterpret_cast<uint64_t *>(externalStorage);
            this->externalStorages.push_back(externalStorage);

            externalStorageAllocProperties.completionValue = this->externalEventCounterValue * this->externalStorages.size();
            externalStorageAllocProperties.deviceAddress = this->externalEventDeviceAddress;
            externalStorageAllocProperties.incrementValue = this->externalEventIncrementValue * this->externalStorages.size();

            counterBasedDesc.pNext = &externalStorageAllocProperties;
        }

        counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
        if (timestamp) {
            counterBasedDesc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;
        }
        if (signalScope) {
            counterBasedDesc.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
        }
        ze_event_handle_t eventHandle = nullptr;
        ze_result_t ret = zexCounterBasedEventCreate2(this->context, this->device, &counterBasedDesc, &eventHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
        if (eventHandle) {
            this->eventHandles.push_back(eventHandle);
            event = Event::fromHandle(eventHandle);
            this->events.push_back(event);
        }
    } else {
        if (!this->eventPoolHandle) {
            ze_event_pool_desc_t eventPoolDesc = {};
            eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
            eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            if (timestamp) {
                eventPoolDesc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            }
            eventPoolDesc.count = 32;
            ze_result_t ret = ZE_RESULT_SUCCESS;
            zeEventPoolCreate(this->context, &eventPoolDesc, 0, nullptr, &this->eventPoolHandle);
            EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
        }
        if (this->eventPoolHandle) {
            ze_event_desc_t eventDesc = {};
            eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
            eventDesc.index = static_cast<uint32_t>(this->eventHandles.size());
            if (signalScope) {
                eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
            }
            ze_event_handle_t eventHandle = nullptr;
            ze_result_t ret = zeEventCreate(this->eventPoolHandle, &eventDesc, &eventHandle);
            EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
            if (eventHandle) {
                this->eventHandles.push_back(eventHandle);
                event = Event::fromHandle(eventHandle);
                this->events.push_back(event);
            }
        }
    }
    return event;
}

void MutableCommandListFixtureInit::resizeKernelArg(uint32_t resize) {
    kernel->kernelArgInfos.resize(resize);
    kernel->isArgUncached.resize(resize);
    kernel->argumentsResidencyContainer.resize(resize);
    kernel->slmArgOffsetValues.resize(resize);
    kernel->slmArgSizes.resize(resize);
    kernel->kernelArgHandlers.resize(resize);
    mockKernelImmData->resizeExplicitArgs(resize);

    kernel2->kernelArgInfos.resize(resize);
    kernel2->isArgUncached.resize(resize);
    kernel2->argumentsResidencyContainer.resize(resize);
    kernel2->slmArgOffsetValues.resize(resize);
    kernel2->slmArgSizes.resize(resize);
    kernel2->kernelArgHandlers.resize(resize);
    mockKernelImmData2->resizeExplicitArgs(resize);
}

void MutableCommandListFixtureInit::prepareKernelArg(uint16_t argIndex, L0::MCL::VariableType varType, uint32_t kernelMask) {
    CrossThreadDataOffset offset = this->crossThreadOffset + (argIndex * this->nextArgOffset);

    if (varType == L0::MCL::VariableType::buffer) {
        NEO::ArgDescriptor kernelArgPtr = {NEO::ArgDescriptor::argTPointer};
        auto &argPtr = kernelArgPtr.as<NEO::ArgDescPointer>();
        argPtr.stateless = offset;
        argPtr.bufferOffset = 0;
        argPtr.bufferSize = offset + 8;
        argPtr.pointerSize = 8;

        if (kernelMask & kernel1Bit) {
            mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgPtr;
        }
        if (kernelMask & kernel2Bit) {
            mockKernelImmData2->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgPtr;
        }
    }
    if (varType == L0::MCL::VariableType::value) {
        NEO::ArgDescriptor kernelArgValue = {NEO::ArgDescriptor::argTValue};
        auto &argValue = kernelArgValue.as<NEO::ArgDescValue>();
        NEO::ArgDescValue::Element element{};
        element.offset = offset;
        element.size = sizeof(uint32_t);
        argValue.elements.push_back(element);

        if (kernelMask & kernel1Bit) {
            mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgValue;
        }
        if (kernelMask & kernel2Bit) {
            mockKernelImmData2->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgValue;
        }
    }
    if (varType == L0::MCL::VariableType::slmBuffer) {
        NEO::ArgDescriptor kernelArgSlm = {NEO::ArgDescriptor::argTPointer};
        kernelArgSlm.getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrLocal;
        auto &argSlm = kernelArgSlm.as<NEO::ArgDescPointer>();
        argSlm.requiredSlmAlignment = 4;
        argSlm.slmOffset = offset;
        argSlm.bufferSize = offset + 8;
        argSlm.pointerSize = 8;

        if (kernelMask & kernel1Bit) {
            memset(ptrOffset(kernel->crossThreadData.get(), argSlm.slmOffset), 0, 8);
            mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgSlm;
        }
        if (kernelMask & kernel2Bit) {
            memset(ptrOffset(kernel2->crossThreadData.get(), argSlm.slmOffset), 0, 8);
            mockKernelImmData2->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgSlm;
        }
    }
}

std::vector<L0::MCL::Variable *> MutableCommandListFixtureInit::getVariableList(uint64_t commandId, L0::MCL::VariableType varType, L0::Kernel *kernelOption) {
    auto &selectedAppend = mutableCommandList->mutations[(commandId - 1)];
    std::vector<L0::MCL::Variable *> selectedVariables;
    L0::MCL::MutationVariables *appendVariableDescriptors = nullptr;
    if (kernelOption != nullptr) {
        for (auto &mutableKernel : selectedAppend.kernelGroup->getKernelsInGroup()) {
            if (mutableKernel->getKernel() == kernelOption) {
                appendVariableDescriptors = &mutableKernel->getKernelVariables();
            }
        }
    } else {
        appendVariableDescriptors = &selectedAppend.variables;
    }
    if (appendVariableDescriptors != nullptr) {
        for (auto &varDesc : *appendVariableDescriptors) {
            if (varDesc.var->getType() == varType) {
                selectedVariables.push_back(varDesc.var);
            }
        }
    }
    return selectedVariables;
}

void MutableCommandListFixtureInit::overridePatchedScratchAddress(uint64_t scratchAddress) {
    auto &cmdsToPatch = mutableCommandList->base->getCommandsToPatch();
    for (auto &cmd : cmdsToPatch) {
        if (cmd.type == CommandToPatch::CommandType::ComputeWalkerInlineDataScratch) {
            cmd.scratchAddressAfterPatch = scratchAddress;
        }
    }
}

} // namespace ult
} // namespace L0
