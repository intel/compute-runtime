/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/mem_obj/mem_obj.h"

#include "memory_properties_flags.h"
#include "neo_igfxfmid.h"

#include <functional>

namespace NEO {
struct UnifiedSharingMemoryDescription;
class Buffer;
class ClDevice;
class Device;
class MemoryManager;
struct EncodeSurfaceStateArgs;

using BufferCreateFunc = Buffer *(*)(Context *context,
                                     const MemoryProperties &memoryProperties,
                                     cl_mem_flags flags,
                                     cl_mem_flags_intel flagsIntel,
                                     size_t size,
                                     void *memoryStorage,
                                     void *hostPtr,
                                     MultiGraphicsAllocation &&multiGraphicsAllocation,
                                     bool zeroCopy,
                                     bool isHostPtrSVM,
                                     bool isImageRedescribed);

struct BufferFactoryFuncs {
    BufferCreateFunc createBufferFunction;
};

extern BufferFactoryFuncs bufferFactory[IGFX_MAX_CORE];

namespace BufferFunctions {
using ValidateInputAndCreateBufferFunc = std::function<cl_mem(cl_context context,
                                                              const uint64_t *properties,
                                                              uint64_t flags,
                                                              uint64_t flagsIntel,
                                                              size_t size,
                                                              void *hostPtr,
                                                              int32_t &retVal)>;
extern ValidateInputAndCreateBufferFunc validateInputAndCreateBuffer;
} // namespace BufferFunctions

class Buffer : public MemObj {
  public:
    struct AdditionalBufferCreateArgs {
        bool doNotProvidePerformanceHints;
        bool makeAllocationLockable;
    };
    constexpr static size_t maxBufferSizeForReadWriteOnCpu = 10 * MemoryConstants::megaByte;
    constexpr static size_t maxBufferSizeForCopyOnCpu = 64 * MemoryConstants::kiloByte;
    constexpr static cl_ulong maskMagic = 0xFFFFFFFFFFFFFFFFLL;
    constexpr static cl_ulong objectMagic = MemObj::objectMagic | 0x02;
    bool forceDisallowCPUCopy = false;

    ~Buffer() override;

    static cl_mem validateInputAndCreateBuffer(cl_context context,
                                               const cl_mem_properties *properties,
                                               cl_mem_flags flags,
                                               cl_mem_flags_intel flagsIntel,
                                               size_t size,
                                               void *hostPtr,
                                               cl_int &retVal);

    static Buffer *create(Context *context,
                          cl_mem_flags flags,
                          size_t size,
                          void *hostPtr,
                          cl_int &errcodeRet);

    static Buffer *create(Context *context,
                          cl_mem_flags flags,
                          size_t size,
                          void *hostPtr,
                          AdditionalBufferCreateArgs &bufferCreateArgs,
                          cl_int &errcodeRet);

    static Buffer *create(Context *context,
                          const MemoryProperties &properties,
                          cl_mem_flags flags,
                          cl_mem_flags_intel flagsIntel,
                          size_t size,
                          void *hostPtr,
                          cl_int &errcodeRet);

    static Buffer *create(Context *context,
                          const MemoryProperties &properties,
                          cl_mem_flags flags,
                          cl_mem_flags_intel flagsIntel,
                          size_t size,
                          void *hostPtr,
                          AdditionalBufferCreateArgs &bufferCreateArgs,
                          cl_int &errcodeRet);

    static Buffer *createSharedBuffer(Context *context,
                                      cl_mem_flags flags,
                                      SharingHandler *sharingHandler,
                                      MultiGraphicsAllocation multiGraphicsAllocation);

    static Buffer *createBufferHw(Context *context,
                                  const MemoryProperties &memoryProperties,
                                  cl_mem_flags flags,
                                  cl_mem_flags_intel flagsIntel,
                                  size_t size,
                                  void *memoryStorage,
                                  void *hostPtr,
                                  MultiGraphicsAllocation &&multiGraphicsAllocation,
                                  bool zeroCopy,
                                  bool isHostPtrSVM,
                                  bool isImageRedescribed);

    static Buffer *createBufferHwFromDevice(const Device *device,
                                            cl_mem_flags flags,
                                            cl_mem_flags_intel flagsIntel,
                                            size_t size,
                                            void *memoryStorage,
                                            void *hostPtr,
                                            MultiGraphicsAllocation &&multiGraphicsAllocation,
                                            size_t offset,
                                            bool zeroCopy,
                                            bool isHostPtrSVM,
                                            bool isImageRedescribed);

    Buffer *createSubBuffer(cl_mem_flags flags,
                            cl_mem_flags_intel flagsIntel,
                            const cl_buffer_region *region,
                            cl_int &errcodeRet);

    static void setSurfaceState(const Device *device,
                                void *surfaceState,
                                bool forceNonAuxMode,
                                bool disableL3,
                                size_t svmSize,
                                void *svmPtr,
                                size_t offset,
                                GraphicsAllocation *gfxAlloc,
                                cl_mem_flags flags,
                                cl_mem_flags_intel flagsIntel,
                                bool areMultipleSubDevicesInContext);

    static void provideCompressionHint(bool compressionEnabled, Context *context, Buffer *buffer);

    BufferCreateFunc createFunction = nullptr;
    bool isSubBuffer();
    bool isValidSubBufferOffset(size_t offset);
    uint64_t setArgStateless(void *memory, uint32_t patchSize, uint32_t rootDeviceIndex, bool set32BitAddressing);
    virtual void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation,
                                bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) = 0;
    bool bufferRectPitchSet(const size_t *bufferOrigin,
                            const size_t *region,
                            size_t &bufferRowPitch,
                            size_t &bufferSlicePitch,
                            size_t &hostRowPitch,
                            size_t &hostSlicePitch,
                            bool isSrcBuffer);

    static size_t calculateHostPtrSize(const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch);

    void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;
    void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;

    bool isReadWriteOnCpuAllowed(const Device &device);
    bool isReadWriteOnCpuPreferred(void *ptr, size_t size, const Device &device);

    uint32_t getMocsValue(bool disableL3Cache, bool isReadOnlyArgument, uint32_t rootDeviceIndex) const;
    uint32_t getSurfaceSize(bool alignSizeForAuxTranslation, uint32_t rootDeviceIndex) const;
    uint64_t getBufferAddress(uint32_t rootDeviceIndex) const;

    bool isCompressed(uint32_t rootDeviceIndex) const;

    static bool validateHandleType(const MemoryProperties &memoryProperties, UnifiedSharingMemoryDescription &extMem);

  protected:
    Buffer(Context *context,
           const MemoryProperties &memoryProperties,
           cl_mem_flags flags,
           cl_mem_flags_intel flagsIntel,
           size_t size,
           void *memoryStorage,
           void *hostPtr,
           MultiGraphicsAllocation &&multiGraphicsAllocation,
           bool zeroCopy,
           bool isHostPtrSVM,
           bool isObjectRedescribed);

    Buffer();

    static void checkMemory(const MemoryProperties &memoryProperties,
                            size_t size,
                            void *hostPtr,
                            cl_int &errcodeRet,
                            bool &isZeroCopy,
                            bool &copyMemoryFromHostPtr,
                            MemoryManager *memMngr,
                            uint32_t rootDeviceIndex,
                            bool forceCopyHostPtr);
    static AllocationType getGraphicsAllocationTypeAndCompressionPreference(const MemoryProperties &properties,
                                                                            bool &compressionEnabled, bool localMemoryEnabled);
    static bool isReadOnlyMemoryPermittedByFlags(const MemoryProperties &properties);

    void transferData(void *dst, void *src, size_t copySize, size_t copyOffset);

    void appendSurfaceStateArgs(EncodeSurfaceStateArgs &args);
};

template <typename GfxFamily>
class BufferHw : public Buffer {
  public:
    BufferHw(Context *context,
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
        : Buffer(context, memoryProperties, flags, flagsIntel, size, memoryStorage, hostPtr, std::move(multiGraphicsAllocation),
                 zeroCopy, isHostPtrSVM, isObjectRedescribed) {}

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation,
                        bool isReadOnlyArgument, const Device &device, bool areMultipleSubDevicesInContext) override;

    static Buffer *create(Context *context,
                          const MemoryProperties &memoryProperties,
                          cl_mem_flags flags,
                          cl_mem_flags_intel flagsIntel,
                          size_t size,
                          void *memoryStorage,
                          void *hostPtr,
                          MultiGraphicsAllocation &&multiGraphicsAllocation,
                          bool zeroCopy,
                          bool isHostPtrSVM,
                          bool isObjectRedescribed) {
        auto buffer = new BufferHw<GfxFamily>(context,
                                              memoryProperties,
                                              flags,
                                              flagsIntel,
                                              size,
                                              memoryStorage,
                                              hostPtr,
                                              std::move(multiGraphicsAllocation),
                                              zeroCopy,
                                              isHostPtrSVM,
                                              isObjectRedescribed);
        buffer->surfaceType = SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
        return buffer;
    }

    typedef typename GfxFamily::RENDER_SURFACE_STATE SURFACE_STATE;
    typename SURFACE_STATE::SURFACE_TYPE surfaceType;
};

} // namespace NEO
