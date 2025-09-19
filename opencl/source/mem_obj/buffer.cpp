/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/buffer.h"

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/migration_sync_data.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/sharings/unified/unified_buffer.h"

namespace NEO {

BufferFactoryFuncs bufferFactory[IGFX_MAX_CORE] = {};

namespace BufferFunctions {
ValidateInputAndCreateBufferFunc validateInputAndCreateBuffer = Buffer::validateInputAndCreateBuffer;
} // namespace BufferFunctions

Buffer::Buffer(Context *context,
               const MemoryProperties &memoryProperties,
               cl_mem_flags flags,
               cl_mem_flags_intel flagsIntel,
               size_t size,
               void *memoryStorage,
               void *hostPtr,
               MultiGraphicsAllocation &&multiGraphicsAllocation,
               bool zeroCopy,
               bool isHostPtrSVM,
               bool isObjectRedescribed)
    : MemObj(context,
             CL_MEM_OBJECT_BUFFER,
             memoryProperties,
             flags,
             flagsIntel,
             size,
             memoryStorage,
             hostPtr,
             std::move(multiGraphicsAllocation),
             zeroCopy,
             isHostPtrSVM,
             isObjectRedescribed) {
    magic = objectMagic;
    setHostPtrMinSize(size);
}

Buffer::Buffer() : MemObj(nullptr, CL_MEM_OBJECT_BUFFER, {}, 0, 0, 0, nullptr, nullptr, 0, false, false, false) {
}

Buffer::~Buffer() = default;

bool Buffer::isSubBuffer() {
    return this->associatedMemObject != nullptr;
}

bool Buffer::isValidSubBufferOffset(size_t offset) {
    if (multiGraphicsAllocation.getDefaultGraphicsAllocation()->isCompressionEnabled()) {
        // From spec: "origin value is aligned to the CL_DEVICE_MEM_BASE_ADDR_ALIGN value"
        if (!isAligned(offset, this->getContext()->getDevice(0)->getDeviceInfo().memBaseAddressAlign / 8u)) {
            return false;
        }
    }
    cl_uint addressAlign = 32; // 4 byte alignment
    if ((offset & (addressAlign / 8 - 1)) == 0) {
        return true;
    }

    return false;
}

cl_mem Buffer::validateInputAndCreateBuffer(cl_context context,
                                            const cl_mem_properties *properties,
                                            cl_mem_flags flags,
                                            cl_mem_flags_intel flagsIntel,
                                            size_t size,
                                            void *hostPtr,
                                            cl_int &retVal) {
    Context *pContext = nullptr;
    retVal = validateObjects(withCastToInternal(context, &pContext));
    if (retVal != CL_SUCCESS) {
        return nullptr;
    }

    if ((flags & CL_MEM_USE_HOST_PTR) && !!debugManager.flags.ForceZeroCopyForUseHostPtr.get()) {
        flags |= CL_MEM_FORCE_HOST_MEMORY_INTEL;
    }

    MemoryProperties memoryProperties{};
    cl_mem_alloc_flags_intel allocflags = 0;
    cl_mem_flags_intel emptyFlagsIntel = 0;
    if ((false == ClMemoryPropertiesHelper::parseMemoryProperties(nullptr, memoryProperties, flags, emptyFlagsIntel, allocflags,
                                                                  ClMemoryPropertiesHelper::ObjType::buffer, *pContext)) ||
        (false == MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, emptyFlagsIntel, *pContext))) {
        retVal = CL_INVALID_VALUE;
        return nullptr;
    }

    if ((false == ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                  ClMemoryPropertiesHelper::ObjType::buffer, *pContext)) ||
        (false == MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, *pContext))) {
        retVal = CL_INVALID_PROPERTY;
        return nullptr;
    }

    auto pDevice = pContext->getDevice(0);
    bool allowCreateBuffersWithUnrestrictedSize = isValueSet(flags, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL) ||
                                                  isValueSet(flagsIntel, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL) ||
                                                  debugManager.flags.AllowUnrestrictedSize.get();

    if (size == 0 || (size > pDevice->getDevice().getDeviceInfo().maxMemAllocSize && !allowCreateBuffersWithUnrestrictedSize)) {
        retVal = CL_INVALID_BUFFER_SIZE;
        return nullptr;
    }

    /* Check the host ptr and data */
    bool expectHostPtr = (flags & (CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR)) != 0;
    if ((hostPtr == nullptr) == expectHostPtr) {
        retVal = CL_INVALID_HOST_PTR;
        return nullptr;
    }

    if (expectHostPtr) {
        auto svmAlloc = pContext->getSVMAllocsManager()->getSVMAlloc(hostPtr);

        if (svmAlloc) {
            auto rootDeviceIndex = pDevice->getRootDeviceIndex();
            auto allocationEndAddress = svmAlloc->gpuAllocations.getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + svmAlloc->size;
            auto bufferEndAddress = castToUint64(hostPtr) + size;

            if ((size > svmAlloc->size) || (bufferEndAddress > allocationEndAddress)) {
                retVal = CL_INVALID_BUFFER_SIZE;
                return nullptr;
            }
        }
    }

    // create the buffer

    Buffer *pBuffer = nullptr;
    UnifiedSharingMemoryDescription extMem{};

    if (memoryProperties.handle) {
        if (validateHandleType(memoryProperties, extMem)) {
            extMem.handle = reinterpret_cast<void *>(memoryProperties.handle);
            extMem.size = size;
            pBuffer = UnifiedBuffer::createSharedUnifiedBuffer(pContext, flags, extMem, &retVal);
        } else {
            retVal = CL_INVALID_PROPERTY;
            return nullptr;
        }
    } else {
        pBuffer = create(pContext, memoryProperties, flags, flagsIntel, size, hostPtr, retVal);
    }

    if (retVal == CL_SUCCESS) {
        pBuffer->storeProperties(properties);
    }

    return pBuffer;
}

Buffer *Buffer::create(Context *context,
                       cl_mem_flags flags,
                       size_t size,
                       void *hostPtr,
                       AdditionalBufferCreateArgs &bufferCreateArgs,
                       cl_int &errcodeRet) {
    return create(context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                  flags, 0, size, hostPtr, bufferCreateArgs, errcodeRet);
}

Buffer *Buffer::create(Context *context,
                       cl_mem_flags flags,
                       size_t size,
                       void *hostPtr,
                       cl_int &errcodeRet) {
    AdditionalBufferCreateArgs bufferCreateArgs{};
    return create(context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                  flags, 0, size, hostPtr, bufferCreateArgs, errcodeRet);
}

bool inline copyHostPointer(Buffer *buffer,
                            Device &device,
                            size_t size,
                            void *hostPtr,
                            bool implicitScalingEnabled,
                            cl_int &errcodeRet) {
    auto rootDeviceIndex = device.getRootDeviceIndex();
    auto &productHelper = device.getProductHelper();
    auto memory = buffer->getGraphicsAllocation(rootDeviceIndex);
    auto isCompressionEnabled = memory->isCompressionEnabled();
    const bool isLocalMemory = !MemoryPoolHelper::isSystemMemoryPool(memory->getMemoryPool());
    const bool gpuCopyRequired = isCompressionEnabled || isLocalMemory;
    if (gpuCopyRequired) {
        auto &hwInfo = device.getHardwareInfo();

        auto &osInterface = device.getRootDeviceEnvironment().osInterface;
        bool isLockable = true;
        if (osInterface) {
            isLockable = osInterface->isLockablePointer(memory->storageInfo.isLockable);
        }

        bool copyOnCpuAllowed = implicitScalingEnabled == false &&
                                size <= Buffer::maxBufferSizeForCopyOnCpu &&
                                isCompressionEnabled == false &&
                                productHelper.getLocalMemoryAccessMode(hwInfo) != LocalMemoryAccessMode::cpuAccessDisallowed &&
                                isLockable;

        if (debugManager.flags.CopyHostPtrOnCpu.get() != -1) {
            copyOnCpuAllowed = debugManager.flags.CopyHostPtrOnCpu.get() == 1;
        }
        if (auto lockedPointer = copyOnCpuAllowed ? device.getMemoryManager()->lockResource(memory) : nullptr) {
            memory->setAubWritable(true, GraphicsAllocation::defaultBank);
            memory->setTbxWritable(true, GraphicsAllocation::defaultBank);
            memcpy_s(ptrOffset(lockedPointer, buffer->getOffset()), size, hostPtr, size);
            return true;
        } else {
            auto blitMemoryToAllocationResult = BlitOperationResult::unsupported;

            if (productHelper.isBlitterFullySupported(hwInfo) && isLocalMemory) {
                device.stopDirectSubmissionForCopyEngine();
                blitMemoryToAllocationResult = BlitHelperFunctions::blitMemoryToAllocation(device, memory, buffer->getOffset(), hostPtr, {size, 1, 1});
            }

            if (blitMemoryToAllocationResult != BlitOperationResult::success) {
                auto context = buffer->getContext();
                auto cmdQ = context->getSpecialQueue(rootDeviceIndex);
                auto mapAllocation = buffer->getMapAllocation(rootDeviceIndex);
                if (CL_SUCCESS != cmdQ->enqueueWriteBuffer(buffer, CL_TRUE, buffer->getOffset(), size, hostPtr, mapAllocation, 0, nullptr, nullptr)) {
                    errcodeRet = CL_OUT_OF_RESOURCES;
                    return false;
                }
            }
            return true;
        }
    } else {
        memcpy_s(ptrOffset(memory->getUnderlyingBuffer(), buffer->getOffset()), size, hostPtr, size);
        return true;
    }
    return false;
}

Buffer *Buffer::create(Context *context,
                       const MemoryProperties &memoryProperties,
                       cl_mem_flags flags,
                       cl_mem_flags_intel flagsIntel,
                       size_t size,
                       void *hostPtr,
                       cl_int &errcodeRet) {
    AdditionalBufferCreateArgs bufferCreateArgs{};
    return create(context, memoryProperties, flags, flagsIntel, size, hostPtr, bufferCreateArgs, errcodeRet);
}

Buffer *Buffer::create(Context *context,
                       const MemoryProperties &memoryProperties,
                       cl_mem_flags flags,
                       cl_mem_flags_intel flagsIntel,
                       size_t size,
                       void *hostPtr,
                       AdditionalBufferCreateArgs &bufferCreateArgs,
                       cl_int &errcodeRet) {

    errcodeRet = CL_SUCCESS;

    RootDeviceIndicesContainer rootDeviceIndices;
    const RootDeviceIndicesContainer *pRootDeviceIndices;
    uint32_t defaultRootDeviceIndex;
    Device *defaultDevice;

    if (memoryProperties.associatedDevices.empty()) {
        defaultDevice = &context->getDevice(0)->getDevice();
        defaultRootDeviceIndex = defaultDevice->getRootDeviceIndex();
        pRootDeviceIndices = &context->getRootDeviceIndices();
    } else {
        for (const auto &device : memoryProperties.associatedDevices) {
            rootDeviceIndices.pushUnique(device->getRootDeviceIndex());
        }
        defaultDevice = memoryProperties.associatedDevices[0];
        defaultRootDeviceIndex = rootDeviceIndices[0];
        pRootDeviceIndices = &rootDeviceIndices;
    }

    Context::BufferPoolAllocator &bufferPoolAllocator = context->getBufferPoolAllocator();
    const bool implicitScalingEnabled = ImplicitScalingHelper::isImplicitScalingEnabled(defaultDevice->getDeviceBitfield(), true);
    const bool useHostPtr = memoryProperties.flags.useHostPtr;
    const bool copyHostPtr = memoryProperties.flags.copyHostPtr;
    if (implicitScalingEnabled == false &&
        useHostPtr == false &&
        memoryProperties.flags.forceHostMemory == false &&
        memoryProperties.associatedDevices.empty()) {
        cl_int poolAllocRet = CL_SUCCESS;
        auto bufferFromPool = bufferPoolAllocator.allocateBufferFromPool(memoryProperties,
                                                                         flags,
                                                                         flagsIntel,
                                                                         size,
                                                                         hostPtr,
                                                                         poolAllocRet);
        if (CL_SUCCESS == poolAllocRet) {
            const bool needsCopy = copyHostPtr;
            if (needsCopy) {
                copyHostPointer(bufferFromPool,
                                *defaultDevice,
                                size,
                                hostPtr,
                                implicitScalingEnabled,
                                poolAllocRet);
            }
            if (!needsCopy || poolAllocRet == CL_SUCCESS) {
                return bufferFromPool;
            } else {
                clReleaseMemObject(bufferFromPool);
            }
        }
    }

    MemoryManager *memoryManager = context->getMemoryManager();
    UNRECOVERABLE_IF(!memoryManager);

    auto maxRootDeviceIndex = context->getMaxRootDeviceIndex();
    auto multiGraphicsAllocation = MultiGraphicsAllocation(maxRootDeviceIndex);

    AllocationInfoType allocationInfos;
    allocationInfos.resize(maxRootDeviceIndex + 1ull);

    void *allocationCpuPtr = nullptr;
    bool forceCopyHostPtr = false;

    for (auto &rootDeviceIndex : *pRootDeviceIndices) {
        allocationInfos[rootDeviceIndex] = {};
        auto &allocationInfo = allocationInfos[rootDeviceIndex];

        auto &executionEnvironment = memoryManager->peekExecutionEnvironment();
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
        auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        auto &defaultProductHelper = defaultDevice->getProductHelper();
        bool compressionSupported = GfxCoreHelper::compressedBuffersSupported(*hwInfo) && !defaultProductHelper.isCompressionForbidden(*hwInfo);

        bool compressionEnabled = MemObjHelper::isSuitableForCompression(compressionSupported, memoryProperties, *context,
                                                                         gfxCoreHelper.isBufferSizeSuitableForCompression(size));

        allocationInfo.allocationType = getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, compressionEnabled,
                                                                                          memoryManager->isLocalMemorySupported(rootDeviceIndex));

        if (allocationCpuPtr) {
            forceCopyHostPtr = !useHostPtr && !copyHostPtr;
            checkMemory(memoryProperties, size, allocationCpuPtr, errcodeRet, allocationInfo.alignmentSatisfied, allocationInfo.copyMemoryFromHostPtr, memoryManager, rootDeviceIndex, forceCopyHostPtr);
        } else {
            checkMemory(memoryProperties, size, hostPtr, errcodeRet, allocationInfo.alignmentSatisfied, allocationInfo.copyMemoryFromHostPtr, memoryManager, rootDeviceIndex, false);
        }

        if (errcodeRet != CL_SUCCESS) {
            cleanAllGraphicsAllocations(*context, *memoryManager, allocationInfos, false);
            return nullptr;
        }

        if (compressionEnabled) {
            allocationInfo.zeroCopyAllowed = false;
            allocationInfo.allocateMemory = true;
        }

        if (useHostPtr) {
            if (allocationInfo.allocationType == AllocationType::bufferHostMemory) {
                if (allocationInfo.alignmentSatisfied) {
                    allocationInfo.zeroCopyAllowed = true;
                    allocationInfo.allocateMemory = false;
                } else {
                    allocationInfo.zeroCopyAllowed = false;
                    allocationInfo.allocateMemory = true;
                }
            }

            if (debugManager.flags.DisableZeroCopyForUseHostPtr.get()) {
                allocationInfo.zeroCopyAllowed = false;
                allocationInfo.allocateMemory = true;
            }

            auto svmManager = context->getSVMAllocsManager();
            if (svmManager) {
                auto svmData = svmManager->getSVMAlloc(hostPtr);
                if (svmData) {
                    if ((svmData->memoryType == InternalMemoryType::hostUnifiedMemory) && memoryManager->isLocalMemorySupported(rootDeviceIndex)) {
                        allocationInfo.memory = nullptr;
                        allocationInfo.allocationType = AllocationType::buffer;
                        allocationInfo.isHostPtrSVM = false;
                        allocationInfo.zeroCopyAllowed = false;
                        allocationInfo.copyMemoryFromHostPtr = true;
                        allocationInfo.allocateMemory = true;
                        allocationInfo.mapAllocation = svmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
                    } else {
                        allocationInfo.memory = svmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
                        allocationInfo.allocationType = allocationInfo.memory->getAllocationType();
                        allocationInfo.isHostPtrSVM = true;
                        allocationInfo.zeroCopyAllowed = allocationInfo.memory->getAllocationType() == AllocationType::svmZeroCopy;
                        allocationInfo.copyMemoryFromHostPtr = false;
                        allocationInfo.allocateMemory = false;
                        allocationInfo.mapAllocation = svmData->cpuAllocation;
                    }
                }
            }
        }

        if (!bufferCreateArgs.doNotProvidePerformanceHints && hostPtr && context->isProvidingPerformanceHints()) {
            if (allocationInfo.zeroCopyAllowed) {
                context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_BUFFER_MEETS_ALIGNMENT_RESTRICTIONS, hostPtr, size);
            } else {
                context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, hostPtr, size, MemoryConstants::pageSize, MemoryConstants::pageSize);
            }
        }

        if (debugManager.flags.DisableZeroCopyForBuffers.get()) {
            allocationInfo.zeroCopyAllowed = false;
        }

        if (!bufferCreateArgs.doNotProvidePerformanceHints && allocationInfo.allocateMemory && context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_BUFFER_NEEDS_ALLOCATE_MEMORY);
        }

        if (!allocationInfo.memory) {
            if (allocationCpuPtr) {
                allocationInfo.allocateMemory = false;
            }

            AllocationProperties allocProperties = MemoryPropertiesHelper::getAllocationProperties(rootDeviceIndex, memoryProperties,
                                                                                                   allocationInfo.allocateMemory, size, allocationInfo.allocationType, context->areMultiStorageAllocationsPreferred(),
                                                                                                   *hwInfo, context->getDeviceBitfieldForAllocation(rootDeviceIndex), context->isSingleDeviceContext());
            allocProperties.flags.preferCompressed = compressionEnabled;
            allocProperties.makeDeviceBufferLockable = bufferCreateArgs.makeAllocationLockable;

            if (allocationCpuPtr) {
                allocationInfo.memory = memoryManager->createGraphicsAllocationFromExistingStorage(allocProperties, allocationCpuPtr, multiGraphicsAllocation);
            } else {
                allocationInfo.memory = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties, hostPtr);
                if (allocationInfo.memory) {
                    allocationCpuPtr = allocationInfo.memory->getUnderlyingBuffer();
                }
            }
        }

        auto isSystemMemory = allocationInfo.memory && MemoryPoolHelper::isSystemMemoryPool(allocationInfo.memory->getMemoryPool());

        if (allocationInfo.allocateMemory && isSystemMemory) {
            memoryManager->addAllocationToHostPtrManager(allocationInfo.memory);
        }

        // if allocation failed for CL_MEM_USE_HOST_PTR case retry with non zero copy path
        if (useHostPtr && !allocationInfo.memory && Buffer::isReadOnlyMemoryPermittedByFlags(memoryProperties)) {
            allocationInfo.allocationType = AllocationType::bufferHostMemory;
            allocationInfo.zeroCopyAllowed = false;
            allocationInfo.copyMemoryFromHostPtr = true;
            AllocationProperties allocProperties = MemoryPropertiesHelper::getAllocationProperties(rootDeviceIndex, memoryProperties,
                                                                                                   true, // allocateMemory
                                                                                                   size, allocationInfo.allocationType, context->areMultiStorageAllocationsPreferred(),
                                                                                                   *hwInfo, context->getDeviceBitfieldForAllocation(rootDeviceIndex), context->isSingleDeviceContext());
            allocationInfo.memory = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);
        }

        if (!allocationInfo.memory) {
            errcodeRet = CL_OUT_OF_HOST_MEMORY;
            cleanAllGraphicsAllocations(*context, *memoryManager, allocationInfos, false);

            return nullptr;
        }

        if (!isSystemMemory) {
            allocationInfo.zeroCopyAllowed = false;
            if (hostPtr) {
                if (!allocationInfo.isHostPtrSVM) {
                    allocationInfo.copyMemoryFromHostPtr = true;
                }
            }
        } else if (!rootDeviceEnvironment.getProductHelper().isNewCoherencyModelSupported() && allocationInfo.allocationType == AllocationType::buffer && !compressionEnabled) {
            allocationInfo.allocationType = AllocationType::bufferHostMemory;
        }

        allocationInfo.memory->setAllocationType(allocationInfo.allocationType);
        auto isWritable = !(memoryProperties.flags.readOnly || memoryProperties.flags.hostReadOnly || memoryProperties.flags.hostNoAccess);
        allocationInfo.memory->setMemObjectsAllocationWithWritableFlags(isWritable);

        multiGraphicsAllocation.addAllocation(allocationInfo.memory);

        if (forceCopyHostPtr) {
            allocationInfo.copyMemoryFromHostPtr = false;
        }
    }

    auto &allocationInfo = allocationInfos[defaultRootDeviceIndex];
    auto allocation = allocationInfo.memory;
    auto memoryStorage = allocation->getUnderlyingBuffer();
    if (pRootDeviceIndices->size() > 1) {
        multiGraphicsAllocation.setMultiStorage(!MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool()));
    }

    auto pBuffer = createBufferHw(context,
                                  memoryProperties,
                                  flags,
                                  flagsIntel,
                                  size,
                                  memoryStorage,
                                  (useHostPtr) ? hostPtr : nullptr,
                                  std::move(multiGraphicsAllocation),
                                  allocationInfo.zeroCopyAllowed,
                                  allocationInfo.isHostPtrSVM,
                                  false);

    if (!pBuffer) {
        errcodeRet = CL_OUT_OF_HOST_MEMORY;
        cleanAllGraphicsAllocations(*context, *memoryManager, allocationInfos, false);

        return nullptr;
    }

    DBG_LOG(LogMemoryObject, __FUNCTION__, "Created Buffer: Handle: ", pBuffer, ", hostPtr: ", hostPtr, ", size: ", size,
            ", memoryStorage: ", allocationInfo.memory->getUnderlyingBuffer(),
            ", GPU address: ", std::hex, allocationInfo.memory->getGpuAddress(),
            ", memoryPool: ", getMemoryPoolString(allocationInfo.memory));

    for (auto &rootDeviceIndex : *pRootDeviceIndices) {
        auto &allocationInfo = allocationInfos[rootDeviceIndex];
        if (useHostPtr) {
            if (!allocationInfo.zeroCopyAllowed && !allocationInfo.isHostPtrSVM) {
                AllocationProperties properties{rootDeviceIndex,
                                                false, // allocateMemory
                                                size, AllocationType::mapAllocation,
                                                false, // isMultiStorageAllocation
                                                context->getDeviceBitfieldForAllocation(rootDeviceIndex)};
                properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = true;
                allocationInfo.mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
            }
        }

        auto isCompressionEnabled = allocationInfo.memory->isCompressionEnabled();
        Buffer::provideCompressionHint(isCompressionEnabled, context, pBuffer);

        if (allocationInfo.mapAllocation) {
            pBuffer->mapAllocations.addAllocation(allocationInfo.mapAllocation);
        }
        pBuffer->setHostPtrMinSize(size);
    }
    if (allocationInfo.copyMemoryFromHostPtr) {
        if (copyHostPointer(pBuffer,
                            *defaultDevice,
                            size,
                            hostPtr,
                            implicitScalingEnabled,
                            errcodeRet)) {
            auto migrationSyncData = pBuffer->getMultiGraphicsAllocation().getMigrationSyncData();
            if (migrationSyncData) {
                migrationSyncData->setCurrentLocation(defaultRootDeviceIndex);
            }
        }
    }

    if (errcodeRet != CL_SUCCESS) {
        pBuffer->release();
        return nullptr;
    }

    if (debugManager.flags.MakeAllBuffersResident.get()) {
        for (size_t deviceNum = 0u; deviceNum < context->getNumDevices(); deviceNum++) {
            auto device = context->getDevice(deviceNum);
            auto graphicsAllocation = pBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
            auto rootDeviceEnvironment = pBuffer->executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()].get();
            rootDeviceEnvironment->memoryOperationsInterface->makeResident(&device->getDevice(), ArrayRef<GraphicsAllocation *>(&graphicsAllocation, 1), false, false);
        }
    }

    return pBuffer;
}

Buffer *Buffer::createSharedBuffer(Context *context, cl_mem_flags flags, SharingHandler *sharingHandler,
                                   MultiGraphicsAllocation multiGraphicsAllocation) {
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();
    auto size = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex)->getUnderlyingBufferSize();
    auto sharedBuffer = createBufferHw(
        context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags, 0, size, nullptr, nullptr, std::move(multiGraphicsAllocation),
        false, false, false);

    sharedBuffer->setSharingHandler(sharingHandler);
    return sharedBuffer;
}

void Buffer::checkMemory(const MemoryProperties &memoryProperties,
                         size_t size,
                         void *hostPtr,
                         cl_int &errcodeRet,
                         bool &alignmentSatisfied,
                         bool &copyMemoryFromHostPtr,
                         MemoryManager *memoryManager,
                         uint32_t rootDeviceIndex,
                         bool forceCopyHostPtr) {
    errcodeRet = CL_SUCCESS;
    alignmentSatisfied = true;
    copyMemoryFromHostPtr = false;
    uintptr_t minAddress = 0;
    auto memRestrictions = memoryManager->getAlignedMallocRestrictions();
    if (memRestrictions) {
        minAddress = memRestrictions->minAddress;
    }

    if (hostPtr) {
        if (!(memoryProperties.flags.useHostPtr || memoryProperties.flags.copyHostPtr || forceCopyHostPtr)) {
            errcodeRet = CL_INVALID_HOST_PTR;
            return;
        }
    }

    if (memoryProperties.flags.useHostPtr) {
        if (hostPtr) {
            auto fragment = memoryManager->getHostPtrManager()->getFragment({hostPtr, rootDeviceIndex});
            if (fragment && fragment->driverAllocation) {
                errcodeRet = CL_INVALID_HOST_PTR;
                return;
            }
            if (alignUp(hostPtr, MemoryConstants::cacheLineSize) != hostPtr ||
                alignUp(size, MemoryConstants::cacheLineSize) != size ||
                minAddress > reinterpret_cast<uintptr_t>(hostPtr)) {
                alignmentSatisfied = false;
                copyMemoryFromHostPtr = true;
            }
        } else {
            errcodeRet = CL_INVALID_HOST_PTR;
        }
    }

    if (memoryProperties.flags.copyHostPtr || forceCopyHostPtr) {
        if (hostPtr) {
            copyMemoryFromHostPtr = true;
        } else {
            errcodeRet = CL_INVALID_HOST_PTR;
        }
    }
}

AllocationType Buffer::getGraphicsAllocationTypeAndCompressionPreference(const MemoryProperties &properties,
                                                                         bool &compressionEnabled, bool isLocalMemoryEnabled) {
    if (properties.flags.forceHostMemory) {
        compressionEnabled = false;
        return AllocationType::bufferHostMemory;
    }

    if (properties.flags.useHostPtr && !isLocalMemoryEnabled) {
        compressionEnabled = false;
        return AllocationType::bufferHostMemory;
    }

    return AllocationType::buffer;
}

bool Buffer::isReadOnlyMemoryPermittedByFlags(const MemoryProperties &properties) {
    // Host won't access or will only read and kernel will only read
    return (properties.flags.hostNoAccess || properties.flags.hostReadOnly) && properties.flags.readOnly;
}

Buffer *Buffer::createSubBuffer(cl_mem_flags flags,
                                cl_mem_flags_intel flagsIntel,
                                const cl_buffer_region *region,
                                cl_int &errcodeRet) {
    DEBUG_BREAK_IF(nullptr == createFunction);
    MemoryProperties memoryProperties =
        ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &this->context->getDevice(0)->getDevice());

    auto copyMultiGraphicsAllocation = MultiGraphicsAllocation{this->multiGraphicsAllocation};
    auto buffer = createFunction(this->context, memoryProperties, flags, 0, region->size,
                                 this->memoryStorage ? ptrOffset(this->memoryStorage, region->origin) : nullptr,
                                 this->hostPtr ? ptrOffset(this->hostPtr, region->origin) : nullptr,
                                 std::move(copyMultiGraphicsAllocation),
                                 this->isZeroCopy, this->isHostPtrSVM, false);

    if (this->context->isProvidingPerformanceHints()) {
        this->context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, SUBBUFFER_SHARES_MEMORY, static_cast<cl_mem>(this));
    }

    buffer->associatedMemObject = this;
    buffer->offset = region->origin + this->offset;
    buffer->setParentSharingHandler(this->getSharingHandler());
    this->incRefInternal();

    errcodeRet = CL_SUCCESS;
    return buffer;
}

uint64_t Buffer::setArgStateless(void *memory, uint32_t patchSize, uint32_t rootDeviceIndex, bool set32BitAddressing) {
    // Subbuffers have offset that graphicsAllocation is not aware of
    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    auto addressToPatch = ((set32BitAddressing) ? graphicsAllocation->getGpuAddressToPatch() : graphicsAllocation->getGpuAddress()) + this->offset;
    DEBUG_BREAK_IF(!(graphicsAllocation->isLocked() || (addressToPatch != 0) || (graphicsAllocation->getGpuBaseAddress() != 0) ||
                     (this->getCpuAddress() == nullptr && graphicsAllocation->peekSharedHandle())));

    patchWithRequiredSize(memory, patchSize, addressToPatch);

    return addressToPatch;
}

bool Buffer::bufferRectPitchSet(const size_t *bufferOrigin,
                                const size_t *region,
                                size_t &bufferRowPitch,
                                size_t &bufferSlicePitch,
                                size_t &hostRowPitch,
                                size_t &hostSlicePitch,
                                bool isSrcBuffer) {
    if (bufferRowPitch == 0)
        bufferRowPitch = region[0];
    if (bufferSlicePitch == 0)
        bufferSlicePitch = region[1] * bufferRowPitch;

    if (hostRowPitch == 0)
        hostRowPitch = region[0];
    if (hostSlicePitch == 0)
        hostSlicePitch = region[1] * hostRowPitch;

    if (region[0] == 0 || region[1] == 0 || region[2] == 0) {
        return false;
    }

    if (bufferRowPitch < region[0] ||
        hostRowPitch < region[0]) {
        return false;
    }
    if ((bufferSlicePitch < region[1] * bufferRowPitch || bufferSlicePitch % bufferRowPitch != 0) ||
        (hostSlicePitch < region[1] * hostRowPitch || hostSlicePitch % hostRowPitch != 0)) {
        return false;
    }
    auto slicePitch = isSrcBuffer ? bufferSlicePitch : hostSlicePitch;
    auto rowPitch = isSrcBuffer ? bufferRowPitch : hostRowPitch;
    if ((bufferOrigin[2] + region[2] - 1) * slicePitch + (bufferOrigin[1] + region[1] - 1) * rowPitch + bufferOrigin[0] + region[0] > this->getSize()) {
        return false;
    }
    return true;
}

void Buffer::transferData(void *dst, void *src, size_t copySize, size_t copyOffset) {
    DBG_LOG(LogMemoryObject, __FUNCTION__, " hostPtr: ", hostPtr, ", size: ", copySize, ", offset: ", copyOffset, ", memoryStorage: ", memoryStorage);
    auto dstPtr = ptrOffset(dst, copyOffset);
    auto srcPtr = ptrOffset(src, copyOffset);
    memcpy_s(dstPtr, copySize, srcPtr, copySize);
}

void Buffer::transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(hostPtr, memoryStorage, copySize[0], copyOffset[0]);
}

void Buffer::transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(memoryStorage, hostPtr, copySize[0], copyOffset[0]);
}

size_t Buffer::calculateHostPtrSize(const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch) {
    size_t hostPtrOffsetInBytes = origin[2] * slicePitch + origin[1] * rowPitch + origin[0];
    size_t hostPtrRegionSizeInbytes = region[0] + rowPitch * (region[1] - 1) + slicePitch * (region[2] - 1);
    size_t hostPtrSize = hostPtrOffsetInBytes + hostPtrRegionSizeInbytes;
    return hostPtrSize;
}

bool Buffer::isReadWriteOnCpuAllowed(const Device &device) {
    if (!allowCpuAccess()) {
        return false;
    }

    if (forceDisallowCPUCopy) {
        return false;
    }

    auto rootDeviceIndex = device.getRootDeviceIndex();

    if (this->isCompressed(rootDeviceIndex)) {
        return false;
    }

    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);

    if (graphicsAllocation->peekSharedHandle() != 0) {
        return false;
    }

    if (graphicsAllocation->isAllocatedInLocalMemoryPool()) {
        return false;
    }

    return true;
}

bool Buffer::isReadWriteOnCpuPreferred(void *ptr, size_t size, const Device &device) {
    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(device.getRootDeviceIndex());
    if (MemoryPoolHelper::isSystemMemoryPool(graphicsAllocation->getMemoryPool())) {
        // if buffer is not zero copy and pointer is aligned it will be more beneficial to do the transfer on GPU
        if (!isMemObjZeroCopy() && (reinterpret_cast<uintptr_t>(ptr) & (MemoryConstants::cacheLineSize - 1)) == 0) {
            return false;
        }

        // on low power devices larger transfers are better on the GPU
        if (device.getSpecializedDevice<ClDevice>()->getDeviceInfo().platformLP && size > maxBufferSizeForReadWriteOnCpu) {
            return false;
        }
        return true;
    }

    return false;
}

Buffer *Buffer::createBufferHw(Context *context,
                               const MemoryProperties &memoryProperties,
                               cl_mem_flags flags,
                               cl_mem_flags_intel flagsIntel,
                               size_t size,
                               void *memoryStorage,
                               void *hostPtr,
                               MultiGraphicsAllocation &&multiGraphicsAllocation,
                               bool zeroCopy,
                               bool isHostPtrSVM,
                               bool isImageRedescribed) {
    const auto device = context->getDevice(0);
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = bufferFactory[hwInfo.platform.eRenderCoreFamily].createBufferFunction;
    DEBUG_BREAK_IF(nullptr == funcCreate);
    auto pBuffer = funcCreate(context, memoryProperties, flags, flagsIntel, size, memoryStorage, hostPtr, std::move(multiGraphicsAllocation),
                              zeroCopy, isHostPtrSVM, isImageRedescribed);
    DEBUG_BREAK_IF(nullptr == pBuffer);
    if (pBuffer) {
        pBuffer->createFunction = funcCreate;
    }
    return pBuffer;
}

Buffer *Buffer::createBufferHwFromDevice(const Device *device,
                                         cl_mem_flags flags,
                                         cl_mem_flags_intel flagsIntel,
                                         size_t size,
                                         void *memoryStorage,
                                         void *hostPtr,
                                         MultiGraphicsAllocation &&multiGraphicsAllocation,
                                         size_t offset,
                                         bool zeroCopy,
                                         bool isHostPtrSVM,
                                         bool isImageRedescribed) {
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = bufferFactory[hwInfo.platform.eRenderCoreFamily].createBufferFunction;
    DEBUG_BREAK_IF(nullptr == funcCreate);

    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, device);
    auto pBuffer = funcCreate(nullptr, memoryProperties, flags, flagsIntel, size, memoryStorage, hostPtr, std::move(multiGraphicsAllocation),
                              zeroCopy, isHostPtrSVM, isImageRedescribed);

    pBuffer->offset = offset;
    pBuffer->executionEnvironment = device->getExecutionEnvironment();
    return pBuffer;
}

uint32_t Buffer::getMocsValue(bool disableL3Cache, bool isReadOnlyArgument, uint32_t rootDeviceIndex) const {
    uint64_t bufferAddress = 0;
    size_t bufferSize = 0;
    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    if (graphicsAllocation) {
        bufferAddress = graphicsAllocation->getGpuAddress();
        bufferSize = graphicsAllocation->getUnderlyingBufferSize();
    } else {
        bufferAddress = reinterpret_cast<uint64_t>(getHostPtr());
        bufferSize = getSize();
    }
    bufferAddress += this->offset;

    bool readOnlyMemObj = isValueSet(getFlags(), CL_MEM_READ_ONLY) || isReadOnlyArgument;
    bool alignedMemObj = isAligned<MemoryConstants::cacheLineSize>(bufferAddress) &&
                         isAligned<MemoryConstants::cacheLineSize>(bufferSize);

    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();
    if (!disableL3Cache && !isMemObjUncacheableForSurfaceState() && (alignedMemObj || readOnlyMemObj || !isMemObjZeroCopy())) {
        return gmmHelper->getL3EnabledMOCS();
    } else {
        return gmmHelper->getUncachedMOCS();
    }
}

uint32_t Buffer::getSurfaceSize(bool alignSizeForAuxTranslation, uint32_t rootDeviceIndex) const {
    auto bufferAddress = getBufferAddress(rootDeviceIndex);
    auto bufferAddressAligned = alignDown(bufferAddress, 4);
    auto bufferOffset = ptrDiff(bufferAddress, bufferAddressAligned);

    uint32_t surfaceSize = static_cast<uint32_t>(alignUp(getSize() + bufferOffset, alignSizeForAuxTranslation ? 512 : 4));
    return surfaceSize;
}

uint64_t Buffer::getBufferAddress(uint32_t rootDeviceIndex) const {
    // The graphics allocation for Host Ptr surface will be created in makeResident call and GPU address is expected to be the same as CPU address
    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    auto bufferAddress = (graphicsAllocation != nullptr) ? graphicsAllocation->getGpuAddress() : castToUint64(getHostPtr());
    bufferAddress += this->offset;
    return bufferAddress;
}

bool Buffer::isCompressed(uint32_t rootDeviceIndex) const {
    return multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex)->isCompressionEnabled();
}

void Buffer::setSurfaceState(const Device *device,
                             void *surfaceState,
                             bool forceNonAuxMode,
                             bool disableL3,
                             size_t svmSize,
                             void *svmPtr,
                             size_t offset,
                             GraphicsAllocation *gfxAlloc,
                             cl_mem_flags flags,
                             cl_mem_flags_intel flagsIntel,
                             bool areMultipleSubDevicesInContext) {
    auto multiGraphicsAllocation = MultiGraphicsAllocation(device->getRootDeviceIndex());
    if (gfxAlloc) {
        multiGraphicsAllocation.addAllocation(gfxAlloc);
    }
    auto buffer = Buffer::createBufferHwFromDevice(device, flags, flagsIntel, svmSize, svmPtr, svmPtr, std::move(multiGraphicsAllocation), offset, true, false, false);
    buffer->setArgStateful(surfaceState, forceNonAuxMode, disableL3, false, false, *device, areMultipleSubDevicesInContext);
    delete buffer;
}

void Buffer::provideCompressionHint(bool compressionEnabled, Context *context, Buffer *buffer) {
    if (context->isProvidingPerformanceHints() && GfxCoreHelper::compressedBuffersSupported(context->getDevice(0)->getHardwareInfo())) {
        if (compressionEnabled) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, BUFFER_IS_COMPRESSED, buffer);
        } else {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, BUFFER_IS_NOT_COMPRESSED, buffer);
        }
    }
}
} // namespace NEO
