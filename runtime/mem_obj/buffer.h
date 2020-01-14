/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/basic_math.h"
#include "core/memory_manager/memory_constants.h"
#include "public/cl_ext_private.h"
#include "runtime/context/context_type.h"
#include "runtime/mem_obj/mem_obj.h"

#include "igfxfmid.h"
#include "memory_properties_flags.h"

namespace NEO {
class Buffer;
class ClDevice;
class MemoryManager;

typedef Buffer *(*BufferCreatFunc)(Context *context,
                                   MemoryPropertiesFlags memoryProperties,
                                   cl_mem_flags flags,
                                   cl_mem_flags_intel flagsIntel,
                                   size_t size,
                                   void *memoryStorage,
                                   void *hostPtr,
                                   GraphicsAllocation *gfxAllocation,
                                   bool zeroCopy,
                                   bool isHostPtrSVM,
                                   bool isImageRedescribed);

typedef struct {
    BufferCreatFunc createBufferFunction;
} BufferFuncs;

extern BufferFuncs bufferFactory[IGFX_MAX_CORE];

class Buffer : public MemObj {
  public:
    constexpr static size_t maxBufferSizeForReadWriteOnCpu = 10 * MB;
    constexpr static cl_ulong maskMagic = 0xFFFFFFFFFFFFFFFFLL;
    constexpr static cl_ulong objectMagic = MemObj::objectMagic | 0x02;
    bool forceDisallowCPUCopy = false;

    ~Buffer() override;

    static void validateInputAndCreateBuffer(cl_context &context,
                                             MemoryPropertiesFlags memoryProperties,
                                             cl_mem_flags flags,
                                             cl_mem_flags_intel flagsIntel,
                                             size_t size,
                                             void *hostPtr,
                                             cl_int &retVal,
                                             cl_mem &buffer);

    static Buffer *create(Context *context,
                          cl_mem_flags flags,
                          size_t size,
                          void *hostPtr,
                          cl_int &errcodeRet);

    static Buffer *create(Context *context,
                          MemoryPropertiesFlags properties,
                          cl_mem_flags flags,
                          cl_mem_flags_intel flagsIntel,
                          size_t size,
                          void *hostPtr,
                          cl_int &errcodeRet);

    static Buffer *createSharedBuffer(Context *context,
                                      cl_mem_flags flags,
                                      SharingHandler *sharingHandler,
                                      GraphicsAllocation *graphicsAllocation);

    static Buffer *createBufferHw(Context *context,
                                  MemoryPropertiesFlags memoryProperties,
                                  cl_mem_flags flags,
                                  cl_mem_flags_intel flagsIntel,
                                  size_t size,
                                  void *memoryStorage,
                                  void *hostPtr,
                                  GraphicsAllocation *gfxAllocation,
                                  bool zeroCopy,
                                  bool isHostPtrSVM,
                                  bool isImageRedescribed);

    static Buffer *createBufferHwFromDevice(const ClDevice *device,
                                            cl_mem_flags flags,
                                            cl_mem_flags_intel flagsIntel,
                                            size_t size,
                                            void *memoryStorage,
                                            void *hostPtr,
                                            GraphicsAllocation *gfxAllocation,
                                            bool zeroCopy,
                                            bool isHostPtrSVM,
                                            bool isImageRedescribed);

    Buffer *createSubBuffer(cl_mem_flags flags,
                            cl_mem_flags_intel flagsIntel,
                            const cl_buffer_region *region,
                            cl_int &errcodeRet);

    static void setSurfaceState(const ClDevice *device,
                                void *surfaceState,
                                size_t svmSize,
                                void *svmPtr,
                                GraphicsAllocation *gfxAlloc = nullptr,
                                cl_mem_flags flags = 0,
                                cl_mem_flags_intel flagsIntel = 0);

    static void provideCompressionHint(GraphicsAllocation::AllocationType allocationType,
                                       Context *context,
                                       Buffer *buffer);

    BufferCreatFunc createFunction = nullptr;
    bool isSubBuffer();
    bool isValidSubBufferOffset(size_t offset);
    uint64_t setArgStateless(void *memory, uint32_t patchSize) { return setArgStateless(memory, patchSize, false); }
    uint64_t setArgStateless(void *memory, uint32_t patchSize, bool set32BitAddressing);
    virtual void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly) = 0;
    bool bufferRectPitchSet(const size_t *bufferOrigin,
                            const size_t *region,
                            size_t &bufferRowPitch,
                            size_t &bufferSlicePitch,
                            size_t &hostRowPitch,
                            size_t &hostSlicePitch);

    static size_t calculateHostPtrSize(const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch);

    void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;
    void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;

    bool isReadWriteOnCpuAllowed(cl_bool blocking, cl_uint numEventsInWaitList, void *ptr, size_t size);

    uint32_t getMocsValue(bool disableL3Cache, bool isReadOnlyArgument) const;

  protected:
    Buffer(Context *context,
           MemoryPropertiesFlags memoryProperties,
           cl_mem_flags flags,
           cl_mem_flags_intel flagsIntel,
           size_t size,
           void *memoryStorage,
           void *hostPtr,
           GraphicsAllocation *gfxAllocation,
           bool zeroCopy,
           bool isHostPtrSVM,
           bool isObjectRedescribed);

    Buffer();

    static void checkMemory(MemoryPropertiesFlags memoryProperties,
                            size_t size,
                            void *hostPtr,
                            cl_int &errcodeRet,
                            bool &isZeroCopy,
                            bool &copyMemoryFromHostPtr,
                            MemoryManager *memMngr);
    static GraphicsAllocation::AllocationType getGraphicsAllocationType(const MemoryPropertiesFlags &properties, Context &context,
                                                                        bool renderCompressedBuffers, bool localMemoryEnabled,
                                                                        bool preferCompression);
    static bool isReadOnlyMemoryPermittedByFlags(const MemoryPropertiesFlags &properties);

    void transferData(void *dst, void *src, size_t copySize, size_t copyOffset);
};

template <typename GfxFamily>
class BufferHw : public Buffer {
  public:
    BufferHw(Context *context,
             MemoryPropertiesFlags memoryProperties,
             cl_mem_flags flags,
             cl_mem_flags_intel flagsIntel,
             size_t size,
             void *memoryStorage,
             void *hostPtr,
             GraphicsAllocation *gfxAllocation,
             bool zeroCopy,
             bool isHostPtrSVM,
             bool isObjectRedescribed)
        : Buffer(context, memoryProperties, flags, flagsIntel, size, memoryStorage, hostPtr, gfxAllocation,
                 zeroCopy, isHostPtrSVM, isObjectRedescribed) {}

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnlyArgument) override;
    void appendBufferState(void *memory, Context *context, GraphicsAllocation *gfxAllocation, bool isReadOnlyArgument);
    void appendSurfaceStateExt(void *memory);

    static Buffer *create(Context *context,
                          MemoryPropertiesFlags memoryProperties,
                          cl_mem_flags flags,
                          cl_mem_flags_intel flagsIntel,
                          size_t size,
                          void *memoryStorage,
                          void *hostPtr,
                          GraphicsAllocation *gfxAllocation,
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
                                              gfxAllocation,
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
