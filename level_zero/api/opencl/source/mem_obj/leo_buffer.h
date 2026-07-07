/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj.h"

namespace NEO {
namespace LEO {

class Buffer : public MemObj {
  public:
    Buffer(Context *context, MemoryProperties &properties, cl_mem_flags flags, void *ptr, void *cpuPtr, size_t size, bool externalHandle) : MemObj(context, properties, flags, cpuPtr, externalHandle, MemObjType::buffer), usmPtr(ptr), bufferSize(size) {};
    Buffer() = delete;
    ~Buffer() override;

    static cl_mem_flags inheritFlags(cl_mem_flags flags, cl_mem_flags parentFlags);
    static Buffer *createSharedBuffer(Context *context, cl_mem_flags flags, SharingHandler *sharingHandler, MultiGraphicsAllocation &&multiGraphicsAllocation);

    cl_mem_object_type getClObjectType() final;
    size_t getApiSize() const final;
    bool isCompressionEnabled() final;
    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) final { return static_cast<GraphicsAllocation *>(this->getAllocData()->gpuAllocations.getGraphicsAllocation(rootDeviceIndex)); };
    void resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation) final;
    void removeGraphicsAllocation(uint32_t rootDeviceIndex) final;
    bool isSubBuffer() const;
    bool isValidSubBufferOffset(size_t offset);
    SvmAllocationData *getAllocData();
    cl_int createMapAllocation();
    cl_int validateSubBufferRegion(const cl_buffer_region *region);
    Buffer *createSubBuffer(cl_mem_flags flags,
                            cl_mem_flags_intel flagsIntel,
                            const cl_buffer_region *region);
    void *getUsmPtr() const;
    void **getUsmPtrRef();

  protected:
    void checkUsageAndReleaseOldAllocation(uint32_t rootDeviceIndex) final;
    void *usmPtr = nullptr;
    SvmAllocationData *allocData = nullptr;
    size_t bufferSize = 0u;
};

} // namespace LEO
} // namespace NEO
