/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/source/cmdlist/cmdlist_host_function_parameters.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/driver_experimental/zex_event.h"

namespace L0 {
namespace ult {

void MutableCommandListFixtureInit::setUp(bool createInOrder, int32_t useSemaphore64) {
    if (useSemaphore64 != -1) {
        NEO::debugManager.flags.Enable64BitSemaphore.set(useSemaphore64);
    }

    this->createInOrder = createInOrder;
    ModuleImmutableDataFixture::setUp();

    if (useSemaphore64 != -1) {
        neoDevice->deviceInfo.semaphore64bCmdSupport = !!useSemaphore64;
    }

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    this->engineGroupType = gfxCoreHelper.getEngineGroupType(neoDevice->getDefaultEngine().getEngineType(), neoDevice->getDefaultEngine().getEngineUsage(), device->getHwInfo());

    mutableCommandList = createMutableCmdList();

    this->qwordInUse = this->mutableCommandList->isQwordInOrderCounter();
    this->sem64bSupport = device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport;
    this->lriRequired = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(this->qwordInUse, this->sem64bSupport);

    mockKernelImmData2 = prepareKernelImmData(0x100);
    module2 = prepareModule(mockKernelImmData2.get());
    kernel2 = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module2.get());
    createKernel(kernel2.get());
    module2->mockKernelImmData->kernelDescriptor->kernelMetadata.kernelName = "test2";

    mockKernelImmData = prepareKernelImmData(0x1000);
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
    kernelBigIsa.reset(nullptr);

    module.reset(nullptr);
    module2.reset(nullptr);
    moduleBigIsa.reset(nullptr);

    mockKernelImmData.reset(nullptr);
    mockKernelImmData2.reset(nullptr);
    mockKernelImmDataBigIsa.reset(nullptr);

    ModuleImmutableDataFixture::tearDown();
}

void MutableCommandListFixtureInit::prepareBigIsaKernel() {
    mockKernelImmDataBigIsa = prepareKernelImmData(0x11000);
    moduleBigIsa = prepareModule(mockKernelImmDataBigIsa.get());
    kernelBigIsa = std::make_unique<ModuleImmutableDataFixture::MockKernel>(moduleBigIsa.get());
    createKernel(kernelBigIsa.get());
    moduleBigIsa->mockKernelImmData->kernelDescriptor->kernelMetadata.kernelName = "testBigIsa";

    kernelBigIsaHandle = kernelBigIsa->toHandle();
}

std::unique_ptr<ModuleImmutableDataFixture::MockImmutableData> MutableCommandListFixtureInit::prepareKernelImmData(uint32_t isaSize) {
    auto immData = std::make_unique<MockImmutableData>(0u, 0u, 0u, isaSize, nextIsaPtr);
    nextIsaPtr += isaSize;
    nextIsaPtr = alignUp(nextIsaPtr, 0x1000);

    immData->kernelDescriptor->kernelAttributes.crossThreadDataSize = crossThreadInitSize;
    immData->kernelDescriptor->kernelAttributes.numLocalIdChannels = 3;
    immData->crossThreadDataSize = crossThreadInitSize;
    immData->crossThreadDataTemplate.reset(new uint8_t[crossThreadInitSize]);
    immData->kernelDescriptor->payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0;
    immData->kernelDescriptor->payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = sizeof(void *);
    immData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.offset = 8;
    immData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = sizeof(void *);
    return immData;
}

std::unique_ptr<ModuleImmutableDataFixture::MockModule> MutableCommandListFixtureInit::prepareModule(MockImmutableData *immData) {
    std::initializer_list<ZebinTestData::AppendElfAdditionalSection> additionalSections = {};
    auto zebinDataOut = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
    const auto &src = zebinDataOut->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;
    auto mod = std::make_unique<ModuleImmutableDataFixture::MockModule>(device, moduleBuildLog, ModuleType::user, 0, immData);
    mod->type = ModuleType::user;
    ze_result_t result = mod->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    return mod;
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

Event *MutableCommandListFixtureInit::createTestEvent(bool cbEvent, bool signalScope, bool timestamp, bool externalMemory, bool externalFlag) {
    Event *event = nullptr;
    if (cbEvent) {
        zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
        zex_counter_based_event_external_storage_properties_t externalStorageAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES};
        if (externalMemory) {
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
        if (externalFlag) {
            counterBasedDesc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_EXTERNAL;
        }
        if (signalScope) {
            counterBasedDesc.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
        }
        ze_event_handle_t eventHandle = nullptr;
        ze_result_t ret = L0::zexCounterBasedEventCreate2(this->context, this->device, &counterBasedDesc, &eventHandle);
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
    kernel->privateState.kernelArgInfos.resize(resize);
    kernel->privateState.isArgUncached.resize(resize);
    kernel->privateState.argumentsResidencyContainer.resize(resize);
    kernel->privateState.slmArgOffsetValues.resize(resize);
    kernel->privateState.slmArgSizes.resize(resize);
    kernel->privateState.kernelArgHandlers.resize(resize);
    mockKernelImmData->resizeExplicitArgs(resize);

    kernel2->privateState.kernelArgInfos.resize(resize);
    kernel2->privateState.isArgUncached.resize(resize);
    kernel2->privateState.argumentsResidencyContainer.resize(resize);
    kernel2->privateState.slmArgOffsetValues.resize(resize);
    kernel2->privateState.slmArgSizes.resize(resize);
    kernel2->privateState.kernelArgHandlers.resize(resize);
    mockKernelImmData2->resizeExplicitArgs(resize);

    this->kernelArgCount = resize;
}

void MutableCommandListFixtureInit::enableCooperativeSyncBuffer(uint32_t kernelMask) {
    CrossThreadDataOffset offset = this->crossThreadOffset + (this->kernelArgCount * this->nextArgOffset);
    if (kernelMask & kernel1Bit) {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;
        mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.syncBufferAddress.stateless = offset;
        mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.syncBufferAddress.pointerSize = sizeof(uint64_t);
    }
    if (kernelMask & kernel2Bit) {
        mockKernelImmData2->kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;
        mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.syncBufferAddress.stateless = offset;
        mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.syncBufferAddress.pointerSize = sizeof(uint64_t);
    }
}

void MutableCommandListFixtureInit::setupGroupCountOffsets(uint32_t kernelMask) {
    CrossThreadDataOffset offset = this->crossThreadOffset + (this->kernelArgCount * this->nextArgOffset) + 64;
    if (kernelMask & kernel1Bit) {
        auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
        dispatchTraits.globalWorkSize[0] = offset + 3 * sizeof(uint32_t);
        dispatchTraits.numWorkGroups[0] = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);
        dispatchTraits.workDim = dispatchTraits.numWorkGroups[0] + 3 * sizeof(uint32_t);
    }

    if (kernelMask & kernel2Bit) {
        auto &dispatchTraits = mockKernelImmData2->kernelDescriptor->payloadMappings.dispatchTraits;
        dispatchTraits.globalWorkSize[0] = offset + 3 * sizeof(uint32_t);
        dispatchTraits.numWorkGroups[0] = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);
        dispatchTraits.workDim = dispatchTraits.numWorkGroups[0] + 3 * sizeof(uint32_t);
    }
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
            memset(&kernel->getCrossThreadDataSpan()[argSlm.slmOffset], 0, 8);
            mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgSlm;
        }
        if (kernelMask & kernel2Bit) {
            memset(&kernel2->getCrossThreadDataSpan()[argSlm.slmOffset], 0, 8);
            mockKernelImmData2->kernelDescriptor->payloadMappings.explicitArgs[argIndex] = kernelArgSlm;
        }
    }
}

std::vector<L0::MCL::Variable *> MutableCommandListFixtureInit::getVariableList(uint64_t commandId, L0::MCL::VariableType varType, L0::Kernel *kernelOption) {
    auto &selectedKernelAppend = mutableCommandList->kernelMutations[(commandId - 1)];
    std::vector<L0::MCL::Variable *> selectedVariables;
    L0::MCL::KernelVariableDescriptor *kernelVariableDescriptors = nullptr;
    if (varType == L0::MCL::VariableType::buffer ||
        varType == L0::MCL::VariableType::value ||
        varType == L0::MCL::VariableType::slmBuffer ||
        varType == L0::MCL::VariableType::globalOffset ||
        varType == L0::MCL::VariableType::groupCount ||
        varType == L0::MCL::VariableType::groupSize) {
        if (kernelOption != nullptr) {
            for (auto &mutableKernel : selectedKernelAppend.kernelGroup->getKernelsInGroup()) {
                if (mutableKernel->getKernel() == kernelOption) {
                    kernelVariableDescriptors = &mutableKernel->getKernelVariables();
                }
            }
        } else {
            kernelVariableDescriptors = &selectedKernelAppend.variables;
        }
        if (kernelVariableDescriptors != nullptr) {
            if (varType == L0::MCL::VariableType::buffer ||
                varType == L0::MCL::VariableType::value ||
                varType == L0::MCL::VariableType::slmBuffer) {
                for (auto &varDesc : kernelVariableDescriptors->kernelArguments) {
                    if (varDesc.kernelArgumentVariable != nullptr &&
                        varType == varDesc.kernelArgumentVariable->getDesc().type) {
                        selectedVariables.push_back(varDesc.kernelArgumentVariable);
                    }
                }
            }
            if (varType == L0::MCL::VariableType::globalOffset && kernelVariableDescriptors->globalOffset != nullptr) {
                selectedVariables.push_back(kernelVariableDescriptors->globalOffset);
            }
            if (varType == L0::MCL::VariableType::groupCount && kernelVariableDescriptors->groupCount != nullptr) {
                selectedVariables.push_back(kernelVariableDescriptors->groupCount);
            }
            if (varType == L0::MCL::VariableType::groupSize && kernelVariableDescriptors->groupSize != nullptr) {
                selectedVariables.push_back(kernelVariableDescriptors->groupSize);
            }
        }
    }
    auto &selectedEventAppend = mutableCommandList->eventMutations[(commandId - 1)];
    if (varType == L0::MCL::VariableType::signalEvent && selectedEventAppend.signalEvent.eventVariable != nullptr) {
        selectedVariables.push_back(selectedEventAppend.signalEvent.eventVariable);
    }
    if (varType == L0::MCL::VariableType::waitEvent) {
        for (auto &varDesc : selectedEventAppend.waitEvents) {
            if (varDesc.eventVariable != nullptr) {
                selectedVariables.push_back(varDesc.eventVariable);
            }
        }
    }
    return selectedVariables;
}

void MutableCommandListFixtureInit::overridePatchedScratchAddress(uint64_t scratchAddress) {
    auto cmdsToPatch = mutableCommandList->base->getCommandsToPatch();
    for (auto &cmd : cmdsToPatch) {
        if (auto *patch = std::get_if<PatchComputeWalkerInlineDataScratch>(&cmd)) {
            patch->scratchAddressAfterPatch = scratchAddress;
        }
    }
}

bool MutableCommandListFixtureInit::isAllocationInMutableResidency(MutableCommandList *mcl, NEO::GraphicsAllocation *allocation) const {
    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mcl->mutableAllocations);
    auto allocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&allocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == allocation;
                                     });
    return allocationIt != whiteBoxAllocations.addedAllocations.end();
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    L0::CmdListWaitEventParameters waitEventParams;
    callbackData->result = this->mutableCommandList->appendBarrier(callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, waitEventParams);
    callbackData->outWaitCmds = waitEventParams.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = waitEventParams.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    uint8_t dstPtr[64] = {};
    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&dstPtr[0]);

    L0::CmdListWaitEventParameters waitEventParams;
    callbackData->result = this->mutableCommandList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, waitEventParams);
    callbackData->outWaitCmds = waitEventParams.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = waitEventParams.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto srcPtr = allocateUsm(64);
    auto dstPtr = allocateUsm(64);

    L0::CmdListMemoryCopyParams memoryParams{};
    callbackData->result = this->mutableCommandList->appendMemoryCopy(dstPtr, srcPtr, 64, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyRegionCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto srcPtr = allocateUsm(64);
    auto dstPtr = allocateUsm(64);
    const ze_copy_region_t region = {0U, 0U, 0U, 1, 1, 0U};

    L0::CmdListMemoryCopyParams memoryParams{};
    callbackData->result = this->mutableCommandList->appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyWithParametersCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto srcPtr = allocateUsm(64);
    auto dstPtr = allocateUsm(64);

    L0::CmdListMemoryCopyParams memoryParams{};
    callbackData->result = this->mutableCommandList->getBase()->appendMemoryCopyWithParameters(dstPtr, srcPtr, 64, nullptr, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyFromContextCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto srcPtr = allocateUsm(64);
    auto dstPtr = allocateUsm(64);

    L0::CmdListMemoryCopyParams memoryParams{};
    callbackData->result = this->mutableCommandList->getBase()->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 64, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto dstPtr = allocateUsm(64);

    uint8_t pattern = 0;

    L0::CmdListMemoryCopyParams memoryParams{};
    callbackData->result = this->mutableCommandList->appendMemoryFill(dstPtr, &pattern, 1, 64, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillWithParametersCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto dstPtr = allocateUsm(64);

    uint8_t pattern = 0;

    L0::CmdListMemoryCopyParams memoryParams{};
    callbackData->result = this->mutableCommandList->getBase()->appendMemoryFillWithParameters(dstPtr, &pattern, 1, 64, nullptr, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto usm = allocateUsm(64);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr));
    callbackData->dstImageHandle = imagePtr->toHandle();

    L0::CmdListMemoryCopyParams memoryParams{};
    ze_image_region_t dstImgRegion = {2, 1, 1, 4, 2, 2};

    callbackData->result = this->mutableCommandList->getBase()->appendImageCopyFromMemory(imagePtr->toHandle(), usm, &dstImgRegion, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryExtCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto usm = allocateUsm(64);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr));
    callbackData->dstImageHandle = imagePtr->toHandle();

    L0::CmdListMemoryCopyParams memoryParams{};
    ze_image_region_t dstImgRegion = {2, 1, 1, 4, 2, 2};

    uint32_t rowPitch = static_cast<uint32_t>(imagePtr->getImageInfo().rowPitch);
    uint32_t slicePitch = rowPitch;

    callbackData->result = this->mutableCommandList->appendImageCopyFromMemoryExt(imagePtr->toHandle(), usm, &dstImgRegion, rowPitch, slicePitch, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto usm = allocateUsm(64);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr));
    callbackData->srcImageHandle = imagePtr->toHandle();

    L0::CmdListMemoryCopyParams memoryParams{};
    ze_image_region_t srcImgRegion = {2, 1, 1, 4, 2, 2};

    callbackData->result = this->mutableCommandList->getBase()->appendImageCopyToMemory(usm, imagePtr->toHandle(), &srcImgRegion, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryExtCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    auto usm = allocateUsm(64);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtr));
    callbackData->srcImageHandle = imagePtr->toHandle();

    L0::CmdListMemoryCopyParams memoryParams{};
    ze_image_region_t srcImgRegion = {2, 1, 1, 4, 2, 2};

    uint32_t rowPitch = static_cast<uint32_t>(imagePtr->getImageInfo().rowPitch);
    uint32_t slicePitch = rowPitch;

    callbackData->result = this->mutableCommandList->appendImageCopyToMemoryExt(usm, imagePtr->toHandle(), &srcImgRegion, rowPitch, slicePitch, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtrSrc = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtrSrc));
    callbackData->srcImageHandle = imagePtrSrc->toHandle();

    L0::Image *imagePtrDst = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtrDst));
    callbackData->dstImageHandle = imagePtrDst->toHandle();

    L0::CmdListMemoryCopyParams memoryParams{};

    callbackData->result = this->mutableCommandList->getBase()->appendImageCopy(imagePtrDst->toHandle(), imagePtrSrc->toHandle(), callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyRegionCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.width = 4;
    zeDesc.height = 2;
    zeDesc.depth = 2;

    L0::Image *imagePtrSrc = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtrSrc));
    callbackData->srcImageHandle = imagePtrSrc->toHandle();

    L0::Image *imagePtrDst = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Image::create(device->getNEODevice()->getHardwareInfo().platform.eProductFamily, device, &zeDesc, &imagePtrDst));
    callbackData->dstImageHandle = imagePtrDst->toHandle();

    L0::CmdListMemoryCopyParams memoryParams{};
    ze_image_region_t imgRegion = {1, 1, 1, 1, 1, 1};

    callbackData->result = this->mutableCommandList->appendImageCopyRegion(imagePtrDst->toHandle(), imagePtrSrc->toHandle(), &imgRegion, &imgRegion, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, memoryParams);
    callbackData->outWaitCmds = memoryParams.waitEventsParameters.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = memoryParams.waitEventsParameters.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    L0::CmdListWaitEventParameters waitEventParams;
    // when signal event is set, then it is cb to force error, then make it here wait event to force error
    if (callbackData->signalEvent != nullptr) {
        callbackData->numWaitEvents = 1;
        callbackData->waitEvents = &callbackData->signalEvent;
    }
    callbackData->result = this->mutableCommandList->appendWaitOnEvents(callbackData->numWaitEvents, callbackData->waitEvents, waitEventParams);
    callbackData->outWaitCmds = waitEventParams.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = waitEventParams.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    L0::CmdListWaitEventParameters waitEventParams;

    auto usm = allocateUsm(256);
    callbackData->result = this->mutableCommandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(usm), callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, waitEventParams);
    callbackData->outWaitCmds = waitEventParams.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = waitEventParams.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendQueryKernelTimestampsCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    L0::CmdListWaitEventParameters waitEventParams;

    auto testEvent = this->createTestEvent(false, false, false, false, false);
    auto testEventHandle = testEvent->toHandle();

    auto usm = allocateUsm(256);

    callbackData->result = this->mutableCommandList->getBase()->appendQueryKernelTimestamps(1, &testEventHandle, usm, nullptr, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, waitEventParams);
    callbackData->outWaitCmds = waitEventParams.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = waitEventParams.skipAddingWaitEventsToResidency;
}

void MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback(MutableWaitEventsOnAppendOperationsData *callbackData) {
    CmdListHostFunctionParameters parameters;

    auto pHostFunction = reinterpret_cast<ze_host_function_callback_t>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);

    callbackData->result = this->mutableCommandList->appendHostFunction(pHostFunction, pUserData, nullptr, callbackData->signalEvent, callbackData->numWaitEvents, callbackData->waitEvents, parameters);
    callbackData->outWaitCmds = parameters.waitEventParams.outWaitCmds;
    callbackData->skipAddingWaitEventsToResidency = parameters.waitEventParams.skipAddingWaitEventsToResidency;
}

} // namespace ult
} // namespace L0
