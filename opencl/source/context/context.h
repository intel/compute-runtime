/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/memory_manager/usm_pool_params.h"
#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/stackvec.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/context/context_type.h"
#include "opencl/source/context/driver_diagnostics.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/destructor_callbacks.h"
#include "opencl/source/mem_obj/map_operations_handler.h"

#include <map>

enum class InternalMemoryType : uint32_t;

namespace NEO {
struct MemoryProperties;
class HeapAllocator;
class AsyncEventsHandler;
class CommandQueue;
class Device;
class Kernel;
class MemoryManager;
class SharingFunctions;
class SVMAllocsManager;
class ProductHelper;
class Program;
class Platform;
class TagAllocatorBase;
class StagingBufferManager;
class ClDevice;

template <>
struct OpenCLObjectMapper<_cl_context> {
    typedef class Context DerivedType;
};

class Context : public BaseObject<_cl_context> {
    using UsmDeviceMemAllocPool = UsmMemAllocPool;

  public:
    using BufferAllocationsVec = StackVec<GraphicsAllocation *, 1>;

    struct BufferPool : public AbstractBuffersPool<BufferPool, Buffer, MemObj> {
        using BaseType = AbstractBuffersPool<BufferPool, Buffer, MemObj>;

        BufferPool(Context *context);
        Buffer *allocate(const MemoryProperties &memoryProperties,
                         cl_mem_flags flags,
                         cl_mem_flags_intel flagsIntel,
                         size_t requestedSize,
                         void *hostPtr,
                         cl_int &errcodeRet);

        const StackVec<NEO::GraphicsAllocation *, 1> &getAllocationsVector();
    };
    static_assert(NEO::NonCopyable<AbstractBuffersPool<BufferPool, Buffer, MemObj>>);

    class BufferPoolAllocator : public AbstractBuffersAllocator<BufferPool, Buffer, MemObj> {
        using BaseType = AbstractBuffersAllocator<BufferPool, Buffer, MemObj>;

      public:
        BufferPoolAllocator() = default;

        bool isAggregatedSmallBuffersEnabled(Context *context) const;
        void initAggregatedSmallBuffers(Context *context);
        Buffer *allocateBufferFromPool(const MemoryProperties &memoryProperties,
                                       cl_mem_flags flags,
                                       cl_mem_flags_intel flagsIntel,
                                       size_t requestedSize,
                                       void *hostPtr,
                                       cl_int &errcodeRet);
        bool flagsAllowBufferFromPool(const cl_mem_flags &flags, const cl_mem_flags_intel &flagsIntel) const;
        static inline uint32_t calculateMaxPoolCount(SmallBuffersParams smallBuffersParams, uint64_t totalMemory, size_t percentOfMemory) {
            const auto maxPoolCount = static_cast<uint32_t>(totalMemory * (percentOfMemory / 100.0) / (smallBuffersParams.aggregatedSmallBuffersPoolSize));
            return maxPoolCount ? maxPoolCount : 1u;
        }

      protected:
        Buffer *allocateFromPools(const MemoryProperties &memoryProperties,
                                  cl_mem_flags flags,
                                  cl_mem_flags_intel flagsIntel,
                                  size_t requestedSize,
                                  void *hostPtr,
                                  cl_int &errcodeRet);
        Context *context{nullptr};
    };

    static const cl_ulong objectMagic = 0xA4234321DC002130LL;

    bool createImpl(const cl_context_properties *properties,
                    const ClDeviceVector &devices,
                    void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *),
                    void *userData, cl_int &errcodeRet);

    template <typename T>
    static T *create(const cl_context_properties *properties,
                     const ClDeviceVector &devices,
                     void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
                     void *data, cl_int &errcodeRet) {

        auto pContext = new T(funcNotify, data);

        if (!pContext->createImpl(properties, devices, funcNotify, data, errcodeRet)) {
            delete pContext;
            pContext = nullptr;
        } else {
            auto &bufferPoolAllocator = pContext->getBufferPoolAllocator();
            if (bufferPoolAllocator.isAggregatedSmallBuffersEnabled(pContext)) {
                bufferPoolAllocator.initAggregatedSmallBuffers(pContext);
            }
        }
        gtpinNotifyContextCreate(pContext);
        return pContext;
    }

    ~Context() override;

    cl_int setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_context, void *),
                                 void *userData);

    cl_int getInfo(cl_context_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    cl_int getSupportedImageFormats(Device *device, cl_mem_flags flags,
                                    cl_mem_object_type imageType, cl_uint numEntries,
                                    cl_image_format *imageFormats, cl_uint *numImageFormats);

    size_t getNumDevices() const;
    bool containsMultipleSubDevices(uint32_t rootDeviceIndex) const;
    ClDevice *getDevice(size_t deviceOrdinal) const;

    MemoryManager *getMemoryManager() const {
        return memoryManager;
    }

    StagingBufferManager *getStagingBufferManager() const {
        return stagingBufferManager;
    }

    SVMAllocsManager *getSVMAllocsManager() const {
        return svmAllocsManager;
    }

    auto &getMapOperationsStorage() { return mapOperationsStorage; }

    cl_int tryGetExistingHostPtrAllocation(const void *ptr,
                                           size_t size,
                                           uint32_t rootDeviceIndex,
                                           GraphicsAllocation *&allocation,
                                           InternalMemoryType &memoryType,
                                           bool &isCpuCopyAllowed);
    cl_int tryGetExistingSvmAllocation(const void *ptr,
                                       size_t size,
                                       uint32_t rootDeviceIndex,
                                       GraphicsAllocation *&allocation,
                                       InternalMemoryType &memoryType,
                                       bool &isCpuCopyAllowed);
    cl_int tryGetExistingMapAllocation(const void *ptr,
                                       size_t size,
                                       GraphicsAllocation *&allocation);

    const RootDeviceIndicesContainer &getRootDeviceIndices() const;

    uint32_t getMaxRootDeviceIndex() const;

    CommandQueue *getSpecialQueue(uint32_t rootDeviceIndex);
    void setSpecialQueue(CommandQueue *commandQueue, uint32_t rootDeviceIndex);
    void overrideSpecialQueueAndDecrementRefCount(CommandQueue *commandQueue, uint32_t rootDeviceIndex);

    template <typename Sharing>
    Sharing *getSharing();

    template <typename Sharing>
    void registerSharing(Sharing *sharing);

    template <typename... Args>
    void providePerformanceHint(cl_diagnostic_verbose_level_intel flags, PerformanceHints performanceHint, Args &&...args) {
        DEBUG_BREAK_IF(contextCallback == nullptr);
        DEBUG_BREAK_IF(driverDiagnostics == nullptr);
        char hint[DriverDiagnostics::maxHintStringSize];
        snprintf_s(hint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[performanceHint], std::forward<Args>(args)..., 0);
        if (driverDiagnostics->validFlags(flags)) {
            if (contextCallback) {
                contextCallback(hint, &flags, sizeof(flags), userData);
            }
            if (debugManager.flags.PrintDriverDiagnostics.get() != -1) {
                printf("\n%s\n", hint);
            }
        }
    }

    template <typename... Args>
    void providePerformanceHintForMemoryTransfer(cl_command_type commandType, bool transferRequired, Args &&...args) {
        cl_diagnostic_verbose_level_intel verboseLevel = transferRequired ? CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL
                                                                          : CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL;
        PerformanceHints hint = driverDiagnostics->obtainHintForTransferOperation(commandType, transferRequired);

        providePerformanceHint(verboseLevel, hint, args...);
    }

    cl_bool isProvidingPerformanceHints() const {
        return driverDiagnostics != nullptr;
    }

    bool getInteropUserSyncEnabled() { return interopUserSync; }
    void setInteropUserSyncEnabled(bool enabled) { interopUserSync = enabled; }
    bool areMultiStorageAllocationsPreferred();
    bool isSingleDeviceContext();

    ContextType peekContextType() const { return contextType; }

    bool isDeviceAssociated(const ClDevice &clDevice) const;
    ClDevice *getSubDeviceByIndex(uint32_t subDeviceIndex) const;

    AsyncEventsHandler &getAsyncEventsHandler() const;

    DeviceBitfield getDeviceBitfieldForAllocation(uint32_t rootDeviceIndex) const;
    bool getResolvesRequiredInKernels() const {
        return resolvesRequiredInKernels;
    }
    void setResolvesRequiredInKernels(bool resolves) {
        resolvesRequiredInKernels = resolves;
    }
    const ClDeviceVector &getDevices() const {
        return devices;
    }
    const std::map<uint32_t, DeviceBitfield> &getDeviceBitfields() const { return deviceBitfields; };

    static Platform *getPlatformFromProperties(const cl_context_properties *properties, cl_int &errcode);
    BufferPoolAllocator &getBufferPoolAllocator() {
        return smallBufferPoolAllocator;
    }
    UsmMemAllocPool &getDeviceMemAllocPool() {
        return usmDeviceMemAllocPool;
    }

    TagAllocatorBase *getMultiRootDeviceTimestampPacketAllocator();
    std::unique_lock<std::mutex> obtainOwnershipForMultiRootDeviceAllocator();
    void setMultiRootDeviceTimestampPacketAllocator(std::unique_ptr<TagAllocatorBase> &allocator);

    void initializeDeviceUsmAllocationPool();

  protected:
    struct BuiltInKernel {
        const char *pSource = nullptr;
        Program *pProgram = nullptr;
        std::once_flag programIsInitialized; // guard for creating+building the program
        Kernel *pKernel = nullptr;

        BuiltInKernel() {
        }
    };

    Context(void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *) = nullptr,
            void *userData = nullptr);

    // OS specific implementation
    void *getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize);

    void setupContextType();

    virtual void initializeManagers();

    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, DeviceBitfield> deviceBitfields;
    std::vector<std::unique_ptr<SharingFunctions>> sharingFunctions;
    ClDeviceVector devices;
    ContextDestructorCallbacks destructorCallbacks;

    const cl_context_properties *properties = nullptr;
    size_t numProperties = 0u;
    void(CL_CALLBACK *contextCallback)(const char *, const void *, size_t, void *) = nullptr;
    void *userData = nullptr;
    MemoryManager *memoryManager = nullptr;
    SVMAllocsManager *svmAllocsManager = nullptr;
    MapOperationsStorage mapOperationsStorage = {};
    StackVec<CommandQueue *, 1> specialQueues;
    DriverDiagnostics *driverDiagnostics = nullptr;
    BufferPoolAllocator smallBufferPoolAllocator;
    UsmDeviceMemAllocPool usmDeviceMemAllocPool;

    uint32_t maxRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    cl_bool preferD3dSharedResources = 0u;
    ContextType contextType = ContextType::CONTEXT_TYPE_DEFAULT;
    std::unique_ptr<TagAllocatorBase> multiRootDeviceTimestampPacketAllocator;
    std::mutex multiRootDeviceAllocatorMtx;

    StagingBufferManager *stagingBufferManager = nullptr;

    bool interopUserSync = false;
    bool resolvesRequiredInKernels = false;
    bool usmPoolInitialized = false;
    bool platformManagersInitialized = false;
};

static_assert(NEO::NonCopyableAndNonMovable<Context>);

} // namespace NEO
