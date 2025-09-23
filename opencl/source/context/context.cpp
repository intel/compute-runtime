/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/staging_buffer_manager.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/sharing_factory.h"

#include <algorithm>
#include <memory>

namespace NEO {

Context::Context(
    void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
    void *data) {
    contextCallback = funcNotify;
    userData = data;
    sharingFunctions.resize(SharingType::MAX_SHARING_VALUE);
}

Context::~Context() {
    gtpinNotifyContextDestroy(static_cast<cl_context>(this));

    if (multiRootDeviceTimestampPacketAllocator.get() != nullptr) {
        multiRootDeviceTimestampPacketAllocator.reset();
    }

    if (smallBufferPoolAllocator.isAggregatedSmallBuffersEnabled(this)) {
        auto &device = this->getDevice(0)->getDevice();
        device.recordPoolsFreed(smallBufferPoolAllocator.getPoolsCount());
        smallBufferPoolAllocator.releasePools();
    }

    usmDeviceMemAllocPool.cleanup();

    delete[] properties;

    for (auto rootDeviceIndex = 0u; rootDeviceIndex < specialQueues.size(); rootDeviceIndex++) {
        if (specialQueues[rootDeviceIndex]) {
            delete specialQueues[rootDeviceIndex];
        }
    }

    if (platformManagersInitialized) {
        this->getDevice(0)->getPlatform()->decActiveContextCount();
    }

    if (driverDiagnostics) {
        delete driverDiagnostics;
    }
    if (memoryManager && memoryManager->isAsyncDeleterEnabled()) {
        memoryManager->getDeferredDeleter()->removeClient();
    }
    destructorCallbacks.invoke(this);
    for (auto &device : devices) {
        device->decRefInternal();
    }
}

cl_int Context::setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_context, void *),
                                      void *userData) {
    std::unique_lock<std::mutex> theLock(mtx);
    destructorCallbacks.add(funcNotify, userData);
    return CL_SUCCESS;
}

cl_int Context::tryGetExistingHostPtrAllocation(const void *ptr,
                                                size_t size,
                                                uint32_t rootDeviceIndex,
                                                GraphicsAllocation *&allocation,
                                                InternalMemoryType &memoryType,
                                                bool &isCpuCopyAllowed) {
    cl_int retVal = tryGetExistingSvmAllocation(ptr, size, rootDeviceIndex, allocation, memoryType, isCpuCopyAllowed);
    if (retVal != CL_SUCCESS || allocation != nullptr) {
        return retVal;
    }

    retVal = tryGetExistingMapAllocation(ptr, size, allocation);
    return retVal;
}

cl_int Context::tryGetExistingSvmAllocation(const void *ptr,
                                            size_t size,
                                            uint32_t rootDeviceIndex,
                                            GraphicsAllocation *&allocation,
                                            InternalMemoryType &memoryType,
                                            bool &isCpuCopyAllowed) {
    if (getSVMAllocsManager()) {
        SvmAllocationData *svmEntry = getSVMAllocsManager()->getSVMAlloc(ptr);
        if (svmEntry) {
            memoryType = svmEntry->memoryType;
            if ((svmEntry->gpuAllocations.getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + svmEntry->size) < (castToUint64(ptr) + size)) {
                return CL_INVALID_OPERATION;
            }
            allocation = svmEntry->cpuAllocation ? svmEntry->cpuAllocation : svmEntry->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
            if (isCpuCopyAllowed) {
                if (svmEntry->memoryType == InternalMemoryType::deviceUnifiedMemory) {
                    isCpuCopyAllowed = false;
                }
            }
        }
    }
    return CL_SUCCESS;
}

cl_int Context::tryGetExistingMapAllocation(const void *ptr,
                                            size_t size,
                                            GraphicsAllocation *&allocation) {
    if (MapInfo mapInfo = {}; mapOperationsStorage.getInfoForHostPtr(ptr, size, mapInfo)) {
        if (mapInfo.graphicsAllocation) {
            allocation = mapInfo.graphicsAllocation;
        }
    }
    return CL_SUCCESS;
}

const RootDeviceIndicesContainer &Context::getRootDeviceIndices() const {
    return rootDeviceIndices;
}

uint32_t Context::getMaxRootDeviceIndex() const {
    return maxRootDeviceIndex;
}

CommandQueue *Context::getSpecialQueue(uint32_t rootDeviceIndex) {
    if (specialQueues[rootDeviceIndex])
        return specialQueues[rootDeviceIndex];

    static std::mutex mtx;
    std::lock_guard lock(mtx);

    if (!specialQueues[rootDeviceIndex]) {
        cl_int errcodeRet = CL_SUCCESS;
        auto device = std::find_if(this->getDevices().begin(), this->getDevices().end(), [rootDeviceIndex](const auto &device) {
            return device->getRootDeviceIndex() == rootDeviceIndex;
        });

        auto commandQueue = CommandQueue::create(this, *device, nullptr, true, errcodeRet);
        DEBUG_BREAK_IF(commandQueue == nullptr);
        DEBUG_BREAK_IF(errcodeRet != CL_SUCCESS);
        overrideSpecialQueueAndDecrementRefCount(commandQueue, rootDeviceIndex);
    }

    return specialQueues[rootDeviceIndex];
}

void Context::setSpecialQueue(CommandQueue *commandQueue, uint32_t rootDeviceIndex) {
    specialQueues[rootDeviceIndex] = commandQueue;
}
void Context::overrideSpecialQueueAndDecrementRefCount(CommandQueue *commandQueue, uint32_t rootDeviceIndex) {
    commandQueue->setIsSpecialCommandQueue(true);

    // decrement ref count that special queue added
    this->decRefInternal();

    // above decRefInternal doesn't delete this
    setSpecialQueue(commandQueue, rootDeviceIndex); // NOLINT(clang-analyzer-cplusplus.NewDelete)
};

bool Context::areMultiStorageAllocationsPreferred() {
    return this->contextType != ContextType::CONTEXT_TYPE_SPECIALIZED;
}

bool Context::createImpl(const cl_context_properties *properties,
                         const ClDeviceVector &inputDevices,
                         void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
                         void *data, cl_int &errcodeRet) {
    errcodeRet = CL_SUCCESS;

    auto propertiesCurrent = properties;
    bool interopUserSync = false;
    int32_t driverDiagnosticsUsed = -1;
    auto sharingBuilder = sharingFactory.build();

    std::unique_ptr<DriverDiagnostics> driverDiagnostics;
    while (propertiesCurrent && *propertiesCurrent) {
        errcodeRet = CL_SUCCESS;

        auto propertyType = propertiesCurrent[0];
        auto propertyValue = propertiesCurrent[1];
        propertiesCurrent += 2;

        switch (propertyType) {
        case CL_CONTEXT_PLATFORM:
            break;
        case CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL:
            driverDiagnosticsUsed = static_cast<int32_t>(propertyValue);
            break;
        case CL_CONTEXT_INTEROP_USER_SYNC:
            interopUserSync = propertyValue > 0;
            break;
        default:
            if (!sharingBuilder->processProperties(propertyType, propertyValue)) {
                errcodeRet = CL_INVALID_PROPERTY;
                return false;
            }
            break;
        }
    }

    auto numProperties = ptrDiff(propertiesCurrent, properties) / sizeof(cl_context_properties);
    cl_context_properties *propertiesNew = nullptr;

    // copy the user properties if there are any
    if (numProperties) {
        propertiesNew = new cl_context_properties[numProperties + 1];
        memcpy_s(propertiesNew, (numProperties + 1) * sizeof(cl_context_properties), properties, numProperties * sizeof(cl_context_properties));
        propertiesNew[numProperties] = 0;
        numProperties++;
    }

    if (debugManager.flags.PrintDriverDiagnostics.get() != -1) {
        driverDiagnosticsUsed = debugManager.flags.PrintDriverDiagnostics.get();
    }
    if (driverDiagnosticsUsed >= 0) {
        driverDiagnostics = std::make_unique<DriverDiagnostics>(static_cast<cl_diagnostic_verbose_level_intel>(driverDiagnosticsUsed));
    }

    this->numProperties = numProperties;
    this->properties = propertiesNew;
    this->setInteropUserSyncEnabled(interopUserSync);

    if (!sharingBuilder->finalizeProperties(*this, errcodeRet)) {
        return false;
    }

    bool containsDeviceWithSubdevices = false;
    for (const auto &device : inputDevices) {
        rootDeviceIndices.pushUnique(device->getRootDeviceIndex());
        containsDeviceWithSubdevices |= device->getNumGenericSubDevices() > 1;
    }

    this->driverDiagnostics = driverDiagnostics.release();
    if (rootDeviceIndices.size() > 1 && containsDeviceWithSubdevices && !debugManager.flags.EnableMultiRootDeviceContexts.get()) {
        DEBUG_BREAK_IF("No support for context with multiple devices with subdevices");
        errcodeRet = CL_OUT_OF_HOST_MEMORY;
        return false;
    }

    devices = inputDevices;
    for (auto &rootDeviceIndex : rootDeviceIndices) {
        DeviceBitfield deviceBitfield{};
        for (const auto &pDevice : devices) {
            if (pDevice->getRootDeviceIndex() == rootDeviceIndex) {
                deviceBitfield |= pDevice->getDeviceBitfield();
            }
            for (auto &engine : pDevice->getDevice().getAllEngines()) {
                engine.commandStreamReceiver->ensureTagAllocationForRootDeviceIndex(rootDeviceIndex);
            }
        }
        deviceBitfields.insert({rootDeviceIndex, deviceBitfield});
    }

    if (devices.size() > 0) {
        maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
        specialQueues.resize(maxRootDeviceIndex + 1u);
        auto device = this->getDevice(0);
        this->memoryManager = device->getMemoryManager();
        if (memoryManager->isAsyncDeleterEnabled()) {
            memoryManager->getDeferredDeleter()->addClient();
        }

        for (auto &device : devices) {
            device->incRefInternal();
        }

        setupContextType();
        initializeManagers();

        smallBufferPoolAllocator.setParams(SmallBuffersParams::getPreferredBufferPoolParams(device->getProductHelper()));
    }

    return true;
}

cl_int Context::getInfo(cl_context_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t valueSize = GetInfo::invalidSourceSize;
    const void *pValue = nullptr;
    cl_uint numDevices;
    cl_uint refCount = 0;
    std::vector<cl_device_id> devIDs;
    auto callGetinfo = true;

    switch (paramName) {
    case CL_CONTEXT_DEVICES:
        valueSize = devices.size() * sizeof(cl_device_id);
        devices.toDeviceIDs(devIDs);
        pValue = devIDs.data();
        break;

    case CL_CONTEXT_NUM_DEVICES:
        numDevices = static_cast<cl_uint>(devices.size());
        valueSize = sizeof(numDevices);
        pValue = &numDevices;
        break;

    case CL_CONTEXT_PROPERTIES:
        valueSize = this->numProperties * sizeof(cl_context_properties);
        pValue = this->properties;
        if (valueSize == 0) {
            callGetinfo = false;
        }

        break;

    case CL_CONTEXT_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        valueSize = sizeof(refCount);
        pValue = &refCount;
        break;

    default:
        pValue = getOsContextInfo(paramName, &valueSize);
        break;
    }

    GetInfoStatus getInfoStatus = GetInfoStatus::success;
    if (callGetinfo) {
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pValue, valueSize);
    }

    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, valueSize, getInfoStatus);

    return retVal;
}

size_t Context::getNumDevices() const {
    return devices.size();
}

bool Context::containsMultipleSubDevices(uint32_t rootDeviceIndex) const {
    return deviceBitfields.at(rootDeviceIndex).count() > 1;
}

ClDevice *Context::getDevice(size_t deviceOrdinal) const {
    return static_cast<ClDevice *>(devices[deviceOrdinal]);
}

cl_int Context::getSupportedImageFormats(
    Device *device,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_image_format *imageFormats,
    cl_uint *numImageFormatsReturned) {
    size_t numImageFormats = 0;

    const bool nv12ExtensionEnabled = device->getSpecializedDevice<ClDevice>()->getDeviceInfo().nv12Extension;
    const bool packedYuvExtensionEnabled = device->getSpecializedDevice<ClDevice>()->getDeviceInfo().packedYuvExtension;

    auto appendImageFormats = [&](ArrayRef<const ClSurfaceFormatInfo> formats) {
        if (imageFormats) {
            size_t offset = numImageFormats;
            for (size_t i = 0; i < formats.size() && offset < numEntries; ++i) {
                imageFormats[offset++] = formats[i].oclImageFormat;
            }
        }
        numImageFormats += formats.size();
    };

    if (flags & CL_MEM_READ_ONLY) {
        if (this->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features) {
            appendImageFormats(SurfaceFormats::readOnly20());
        } else {
            appendImageFormats(SurfaceFormats::readOnly12());
        }
        if (Image::isImage2d(imageType) && nv12ExtensionEnabled) {
            appendImageFormats(SurfaceFormats::planarYuv());
        }
        if (Image::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readOnlyDepth());
        }
        if (Image::isImage2d(imageType) && packedYuvExtensionEnabled) {
            appendImageFormats(SurfaceFormats::packedYuv());
        }
    } else if (flags & CL_MEM_WRITE_ONLY) {
        appendImageFormats(SurfaceFormats::writeOnly());
        if (Image::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readWriteDepth());
        }
    } else if (nv12ExtensionEnabled && (flags & CL_MEM_NO_ACCESS_INTEL)) {
        if (this->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features) {
            appendImageFormats(SurfaceFormats::readOnly20());
        } else {
            appendImageFormats(SurfaceFormats::readOnly12());
        }
        if (Image::isImage2d(imageType)) {
            appendImageFormats(SurfaceFormats::planarYuv());
        }
    } else {
        appendImageFormats(SurfaceFormats::readWrite());
        if (Image::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readWriteDepth());
        }
    }
    if (numImageFormatsReturned) {
        *numImageFormatsReturned = static_cast<cl_uint>(numImageFormats);
    }
    return CL_SUCCESS;
}

bool Context::isDeviceAssociated(const ClDevice &clDevice) const {
    for (const auto &pDevice : devices) {
        if (pDevice == &clDevice) {
            return true;
        }
    }
    return false;
}

ClDevice *Context::getSubDeviceByIndex(uint32_t subDeviceIndex) const {

    auto isExpectedSubDevice = [subDeviceIndex](ClDevice *pClDevice) -> bool {
        bool isSubDevice = (pClDevice->getDeviceInfo().parentDevice != nullptr);
        if (isSubDevice == false) {
            return false;
        }

        auto &subDevice = static_cast<SubDevice &>(pClDevice->getDevice());
        return (subDevice.getSubDeviceIndex() == subDeviceIndex);
    };

    auto foundDeviceIterator = std::find_if(devices.begin(), devices.end(), isExpectedSubDevice);
    return (foundDeviceIterator != devices.end() ? *foundDeviceIterator : nullptr);
}

AsyncEventsHandler &Context::getAsyncEventsHandler() const {
    return *static_cast<ClExecutionEnvironment *>(devices[0]->getExecutionEnvironment())->getAsyncEventsHandler();
}

DeviceBitfield Context::getDeviceBitfieldForAllocation(uint32_t rootDeviceIndex) const {
    return deviceBitfields.at(rootDeviceIndex);
}

void Context::setupContextType() {
    if (contextType == ContextType::CONTEXT_TYPE_DEFAULT) {
        if (devices.size() > 1) {
            for (const auto &pDevice : devices) {
                if (!pDevice->getDeviceInfo().parentDevice) {
                    contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
                    return;
                }
            }
        }
        if (devices[0]->getDeviceInfo().parentDevice) {
            contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
        }
    }
}

Platform *Context::getPlatformFromProperties(const cl_context_properties *properties, cl_int &errcode) {
    errcode = CL_SUCCESS;
    auto propertiesCurrent = properties;
    while (propertiesCurrent && *propertiesCurrent) {
        auto propertyType = propertiesCurrent[0];
        auto propertyValue = propertiesCurrent[1];
        propertiesCurrent += 2;
        if (CL_CONTEXT_PLATFORM == propertyType) {
            Platform *pPlatform = nullptr;
            errcode = validateObject(withCastToInternal(reinterpret_cast<cl_platform_id>(propertyValue), &pPlatform));
            return pPlatform;
        }
    }
    return nullptr;
}

bool Context::isSingleDeviceContext() {
    return getNumDevices() == 1 && devices[0]->getNumGenericSubDevices() == 0;
}

void Context::initializeDeviceUsmAllocationPool() {
    if (this->usmPoolInitialized) {
        return;
    }
    auto svmMemoryManager = getSVMAllocsManager();
    if (!(svmMemoryManager && this->isSingleDeviceContext())) {
        return;
    }

    TakeOwnershipWrapper<Context> lock(*this);
    if (this->usmPoolInitialized) {
        return;
    }

    auto &productHelper = getDevices()[0]->getProductHelper();
    bool enabled = ApiSpecificConfig::isDeviceUsmPoolingEnabled() &&
                   productHelper.isDeviceUsmPoolAllocatorSupported() &&
                   DeviceFactory::isHwModeSelected();

    auto usmDevicePoolParams = UsmPoolParams::getUsmPoolParams(getDevices()[0]->getGfxCoreHelper());
    if (debugManager.flags.EnableDeviceUsmAllocationPool.get() != -1) {
        enabled = debugManager.flags.EnableDeviceUsmAllocationPool.get() > 0;
        usmDevicePoolParams.poolSize = debugManager.flags.EnableDeviceUsmAllocationPool.get() * MemoryConstants::megaByte;
    }
    if (enabled) {
        auto subDeviceBitfields = getDeviceBitfields();
        auto &neoDevice = devices[0]->getDevice();
        subDeviceBitfields[neoDevice.getRootDeviceIndex()] = neoDevice.getDeviceBitfield();
        SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::deviceUnifiedMemory, MemoryConstants::pageSize2M,
                                                                   getRootDeviceIndices(), subDeviceBitfields);
        memoryProperties.device = &neoDevice;
        usmDeviceMemAllocPool.initialize(svmMemoryManager, memoryProperties, usmDevicePoolParams.poolSize, usmDevicePoolParams.minServicedSize, usmDevicePoolParams.maxServicedSize);
    }
    this->usmPoolInitialized = true;
}

bool Context::BufferPoolAllocator::isAggregatedSmallBuffersEnabled(Context *context) const {
    bool isSupportedForSingleDeviceContexts = false;
    bool isSupportedForAllContexts = false;
    if (context->getNumDevices() > 0) {
        auto ailConfiguration = context->getDevices()[0]->getRootDeviceEnvironment().getAILConfigurationHelper();
        auto &productHelper = context->getDevices()[0]->getProductHelper();
        isSupportedForSingleDeviceContexts = productHelper.isBufferPoolAllocatorSupported() && (ailConfiguration ? ailConfiguration->isBufferPoolEnabled() : true);
    }

    if (debugManager.flags.ExperimentalSmallBufferPoolAllocator.get() != -1) {
        isSupportedForSingleDeviceContexts = debugManager.flags.ExperimentalSmallBufferPoolAllocator.get() >= 1;
        isSupportedForAllContexts = debugManager.flags.ExperimentalSmallBufferPoolAllocator.get() >= 2;
    }

    return isSupportedForAllContexts ||
           (isSupportedForSingleDeviceContexts && context->isSingleDeviceContext());
}

Context::BufferPool::BufferPool(Context *context) : BaseType(context->memoryManager,
                                                             nullptr,
                                                             SmallBuffersParams::getPreferredBufferPoolParams(context->getDevice(0)->getDevice().getProductHelper())) {
    static constexpr cl_mem_flags flags = CL_MEM_UNCOMPRESSED_HINT_INTEL;
    [[maybe_unused]] cl_int errcodeRet{};
    Buffer::AdditionalBufferCreateArgs bufferCreateArgs{};
    bufferCreateArgs.doNotProvidePerformanceHints = true;
    bufferCreateArgs.makeAllocationLockable = true;
    this->mainStorage.reset(Buffer::create(context,
                                           flags,
                                           context->getBufferPoolAllocator().getParams().aggregatedSmallBuffersPoolSize,
                                           nullptr,
                                           bufferCreateArgs,
                                           errcodeRet));
    if (this->mainStorage) {
        this->chunkAllocator.reset(new HeapAllocator(params.startingOffset,
                                                     context->getBufferPoolAllocator().getParams().aggregatedSmallBuffersPoolSize,
                                                     context->getBufferPoolAllocator().getParams().chunkAlignment));
        context->decRefInternal();
    }
}

const StackVec<NEO::GraphicsAllocation *, 1> &Context::BufferPool::getAllocationsVector() {
    return this->mainStorage->getMultiGraphicsAllocation().getGraphicsAllocations();
}

Buffer *Context::BufferPool::allocate(const MemoryProperties &memoryProperties,
                                      cl_mem_flags flags,
                                      cl_mem_flags_intel flagsIntel,
                                      size_t requestedSize,
                                      void *hostPtr,
                                      cl_int &errcodeRet) {
    cl_buffer_region bufferRegion{};
    size_t actualSize = requestedSize;
    bufferRegion.origin = static_cast<size_t>(this->chunkAllocator->allocate(actualSize));
    if (bufferRegion.origin == 0) {
        return nullptr;
    }
    bufferRegion.origin -= params.startingOffset;
    bufferRegion.size = requestedSize;
    auto bufferFromPool = this->mainStorage->createSubBuffer(flags, flagsIntel, &bufferRegion, errcodeRet);
    bufferFromPool->createFunction = this->mainStorage->createFunction;
    bufferFromPool->setSizeInPoolAllocator(actualSize);
    return bufferFromPool;
}

void Context::BufferPoolAllocator::initAggregatedSmallBuffers(Context *context) {
    this->context = context;
    auto &device = context->getDevice(0)->getDevice();
    if (device.requestPoolCreate(1u)) {
        this->addNewBufferPool(Context::BufferPool{this->context});
    }
}

Buffer *Context::BufferPoolAllocator::allocateBufferFromPool(const MemoryProperties &memoryProperties,
                                                             cl_mem_flags flags,
                                                             cl_mem_flags_intel flagsIntel,
                                                             size_t requestedSize,
                                                             void *hostPtr,
                                                             cl_int &errcodeRet) {
    errcodeRet = CL_MEM_OBJECT_ALLOCATION_FAILURE;
    if (this->bufferPools.empty() ||
        !this->isSizeWithinThreshold(requestedSize) ||
        !flagsAllowBufferFromPool(flags, flagsIntel)) {
        return nullptr;
    }

    auto lock = std::unique_lock<std::mutex>(mutex);
    auto bufferFromPool = this->allocateFromPools(memoryProperties, flags, flagsIntel, requestedSize, hostPtr, errcodeRet);
    if (bufferFromPool != nullptr) {
        return bufferFromPool;
    }

    this->drain();

    bufferFromPool = this->allocateFromPools(memoryProperties, flags, flagsIntel, requestedSize, hostPtr, errcodeRet);
    if (bufferFromPool != nullptr) {
        return bufferFromPool;
    }

    auto &device = context->getDevice(0)->getDevice();
    if (device.requestPoolCreate(1u)) {
        this->addNewBufferPool(BufferPool{this->context});
        return this->allocateFromPools(memoryProperties, flags, flagsIntel, requestedSize, hostPtr, errcodeRet);
    }
    return nullptr;
}

Buffer *Context::BufferPoolAllocator::allocateFromPools(const MemoryProperties &memoryProperties,
                                                        cl_mem_flags flags,
                                                        cl_mem_flags_intel flagsIntel,
                                                        size_t requestedSize,
                                                        void *hostPtr,
                                                        cl_int &errcodeRet) {
    for (auto &bufferPoolParent : this->bufferPools) {
        auto &bufferPool = static_cast<BufferPool &>(bufferPoolParent);
        auto bufferFromPool = bufferPool.allocate(memoryProperties, flags, flagsIntel, requestedSize, hostPtr, errcodeRet);
        if (bufferFromPool != nullptr) {
            return bufferFromPool;
        }
    }

    return nullptr;
}

void Context::initializeManagers() {
    auto platform = this->getDevice(0)->getPlatform();
    platform->incActiveContextCount();
    platformManagersInitialized = true;
    this->svmAllocsManager = platform->getSVMAllocsManager();
    this->stagingBufferManager = platform->getStagingBufferManager();
}

TagAllocatorBase *Context::getMultiRootDeviceTimestampPacketAllocator() {
    return multiRootDeviceTimestampPacketAllocator.get();
}
void Context::setMultiRootDeviceTimestampPacketAllocator(std::unique_ptr<TagAllocatorBase> &allocator) {
    multiRootDeviceTimestampPacketAllocator = std::move(allocator);
}

std::unique_lock<std::mutex> Context::obtainOwnershipForMultiRootDeviceAllocator() {
    return std::unique_lock<std::mutex>(multiRootDeviceAllocatorMtx);
}

} // namespace NEO
