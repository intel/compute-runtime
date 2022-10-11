/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/string.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/heap_allocator.h"

#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/context/context_type.h"
#include "opencl/source/context/driver_diagnostics.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/destructor_callbacks.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/map_operations_handler.h"

#include <map>

namespace NEO {

class AsyncEventsHandler;
class CommandQueue;
class Device;
class Kernel;
class MemoryManager;
class SharingFunctions;
class SVMAllocsManager;
class Program;
class Platform;

template <>
struct OpenCLObjectMapper<_cl_context> {
    typedef class Context DerivedType;
};

class Context : public BaseObject<_cl_context> {
  public:
    class BufferPoolAllocator {
      public:
        static constexpr auto aggregatedSmallBuffersPoolSize = 64 * KB;
        static constexpr auto smallBufferThreshold = 4 * KB;
        static constexpr auto chunkAlignment = 256u;
        static constexpr auto startingOffset = chunkAlignment;

        static_assert(aggregatedSmallBuffersPoolSize > smallBufferThreshold, "Largest allowed buffer needs to fit in pool");
        Buffer *allocateBufferFromPool(const MemoryProperties &memoryProperties,
                                       cl_mem_flags flags,
                                       cl_mem_flags_intel flagsIntel,
                                       size_t size,
                                       void *hostPtr,
                                       cl_int &errcodeRet);
        void tryFreeFromPoolBuffer(MemObj *possiblePoolBuffer, size_t offset, size_t size);
        void releaseSmallBufferPool();

        inline bool isAggregatedSmallBuffersEnabled() const {
            constexpr bool enable = false;
            if (DebugManager.flags.ExperimentalSmallBufferPoolAllocator.get() != -1) {
                return !!DebugManager.flags.ExperimentalSmallBufferPoolAllocator.get();
            }
            return enable;
        }
        void initAggregatedSmallBuffers(Context *context);

        bool isPoolBuffer(const MemObj *buffer) const;

      protected:
        inline bool isSizeWithinThreshold(size_t size) const {
            return BufferPoolAllocator::smallBufferThreshold >= size;
        }
        Buffer *mainStorage{nullptr};
        std::unique_ptr<HeapAllocator> chunkAllocator;
        std::mutex mutex;
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
            if (bufferPoolAllocator.isAggregatedSmallBuffersEnabled()) {
                bufferPoolAllocator.initAggregatedSmallBuffers(pContext);
            }
        }
        gtpinNotifyContextCreate(pContext);
        return pContext;
    }

    Context &operator=(const Context &) = delete;
    Context(const Context &) = delete;

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
    void providePerformanceHint(cl_diagnostics_verbose_level flags, PerformanceHints performanceHint, Args &&...args) {
        DEBUG_BREAK_IF(contextCallback == nullptr);
        DEBUG_BREAK_IF(driverDiagnostics == nullptr);
        char hint[DriverDiagnostics::maxHintStringSize];
        snprintf_s(hint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[performanceHint], std::forward<Args>(args)..., 0);
        if (driverDiagnostics->validFlags(flags)) {
            if (contextCallback) {
                contextCallback(hint, &flags, sizeof(flags), userData);
            }
            if (DebugManager.flags.PrintDriverDiagnostics.get() != -1) {
                printf("\n%s\n", hint);
            }
        }
    }

    template <typename... Args>
    void providePerformanceHintForMemoryTransfer(cl_command_type commandType, bool transferRequired, Args &&...args) {
        cl_diagnostics_verbose_level verboseLevel = transferRequired ? CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL
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
        return this->smallBufferPoolAllocator;
    }

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

    uint32_t maxRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    cl_bool preferD3dSharedResources = 0u;
    ContextType contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    bool interopUserSync = false;
    bool resolvesRequiredInKernels = false;
};
} // namespace NEO
