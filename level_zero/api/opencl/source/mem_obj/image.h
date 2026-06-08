/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/mem_obj/mem_obj.h"
#include "level_zero/core/source/image/image_imp.h"

#include <array>
#include <map>

namespace NEO {
namespace LEO {

class Image : public MemObj {
  public:
    Image(Context *context, MemoryProperties &properties, cl_mem_flags flags, ze_image_handle_t imageHandle, void *cpuPtr, ze_image_handle_t baseImageHandle, bool externalHandle, cl_image_format originalFormat, cl_mem memObject) : MemObj(context, properties, flags, cpuPtr, externalHandle, MemObjType::image), imageHandle(imageHandle), baseImageHandle(baseImageHandle), originalFormat(originalFormat) {
        this->associatedMemObject = memObject ? castToObject<MemObj>(memObject) : nullptr;
    };
    Image() = delete;
    ~Image() override;

    static ze_image_type_t clToL0ImageType(cl_mem_object_type clType);
    static void clToL0ImageFormat(ze_image_format_t &l0Format, cl_channel_order clChannelOrder, cl_channel_type clChannelType);
    static bool isSRGB(cl_channel_order clChannelOrder);
    static const ClSurfaceFormatInfo *getSurfaceFormatFromTable(cl_mem_flags flags, const cl_image_format *imageFormat);

    cl_int getImageInfo(cl_image_info paramName,
                        size_t paramValueSize,
                        void *paramValue,
                        size_t *paramValueSizeRet);

    size_t calculateTotalSizeForImage(const std::array<size_t, 3> &sizes) const;

    void getOsSpecificImageInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam);

    cl_mem_object_type getClObjectType() final;
    size_t getApiSize() const final;
    size_t getHostptrSize() const;
    bool isCompressionEnabled() final;
    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) final;
    void resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation) final {};
    void removeGraphicsAllocation(uint32_t rootDeviceIndex) final {};
    void checkUsageAndReleaseOldAllocation(uint32_t rootDeviceIndex) final {};

    ze_image_handle_t getL0Handle() const { return this->imageHandle; }
    ze_image_handle_t getL0Handle(uint32_t rootDeviceIndex) const {
        auto it = this->perDeviceImageHandles.find(rootDeviceIndex);
        return it == this->perDeviceImageHandles.end() ? this->imageHandle : it->second;
    }
    void addPerDeviceHandle(uint32_t rootDeviceIndex, ze_image_handle_t handle) { this->perDeviceImageHandles[rootDeviceIndex] = handle; }
    L0::ImageImp *getL0Object() const { return static_cast<L0::ImageImp *>(L0::Image::fromHandle(this->imageHandle)); }
    L0::ImageImp *getL0Object(uint32_t rootDeviceIndex) const { return static_cast<L0::ImageImp *>(L0::Image::fromHandle(this->getL0Handle(rootDeviceIndex))); }
    cl_image_format getOriginalFormat() const { return this->originalFormat; }
    ze_image_handle_t *getL0HandleRef() { return &this->imageHandle; }

    bool isMultiDevice() const { return !this->perDeviceImageHandles.empty(); }
    void setOwnerRootDeviceIndex(uint32_t rootDeviceIndex) { this->currentOwnerRootDeviceIndex = rootDeviceIndex; }
    void migrateTo(ze_command_list_handle_t cmdList, uint32_t targetRootDeviceIndex, bool outOfOrder,
                   uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);

    size_t getHostPtrRowPitch() const { return hostPtrRowPitch; }
    size_t getHostPtrSlicePitch() const { return hostPtrSlicePitch; }
    void setHostPtrRowPitch(size_t pitch) { hostPtrRowPitch = pitch; }
    void setHostPtrSlicePitch(size_t pitch) { hostPtrSlicePitch = pitch; }

  protected:
    cl_uint mediaPlane = 0;
    ze_image_handle_t imageHandle = nullptr;
    std::map<uint32_t, ze_image_handle_t> perDeviceImageHandles{};
    uint32_t currentOwnerRootDeviceIndex = 0;
    ze_image_handle_t baseImageHandle = nullptr;
    cl_image_format originalFormat = {};
    size_t hostPtrRowPitch = 0;
    size_t hostPtrSlicePitch = 0;
};

} // namespace LEO
} // namespace NEO
