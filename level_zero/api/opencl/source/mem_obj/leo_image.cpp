/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/mem_obj/leo_image.h"

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/helpers/leo_get_info_status_mapper.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include <level_zero/ze_api.h>

#include "CL/cl_gl.h"

namespace NEO {
namespace LEO {

Image::~Image() {
    if (!externalHandle && this->imageHandle) {
        UNRECOVERABLE_IF(zeImageDestroy(this->imageHandle) != ZE_RESULT_SUCCESS);
    }
    for (auto &[rootDeviceIndex, handle] : this->perDeviceImageHandles) {
        zeImageDestroy(handle);
    }
    if (this->baseImageHandle) {
        UNRECOVERABLE_IF(zeImageDestroy(this->baseImageHandle) != ZE_RESULT_SUCCESS);
    }
}

ze_image_type_t Image::clToL0ImageType(cl_mem_object_type clType) {
    switch (clType) {
    case CL_MEM_OBJECT_IMAGE1D:
        return ze_image_type_t::ZE_IMAGE_TYPE_1D;
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        return ze_image_type_t::ZE_IMAGE_TYPE_BUFFER;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        return ze_image_type_t::ZE_IMAGE_TYPE_1DARRAY;
    case CL_MEM_OBJECT_IMAGE2D:
        return ze_image_type_t::ZE_IMAGE_TYPE_2D;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        return ze_image_type_t::ZE_IMAGE_TYPE_2DARRAY;
    case CL_MEM_OBJECT_IMAGE3D:
        return ze_image_type_t::ZE_IMAGE_TYPE_3D;
    default:
        return ze_image_type_t::ZE_IMAGE_TYPE_FORCE_UINT32;
    }
}

bool Image::isSRGB(cl_channel_order clChannelOrder) {
    return clChannelOrder == CL_sRGB ||
           clChannelOrder == CL_sRGBA ||
           clChannelOrder == CL_sBGRA;
}

void Image::clToL0ImageFormat(ze_image_format_t &l0Format, cl_channel_order clChannelOrder, cl_channel_type clChannelType) {
    int channelNumber = 0;
    switch (clChannelOrder) {
    case CL_R:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
        channelNumber = 1;
        break;
    case CL_A:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
        channelNumber = 1;
        break;
    case CL_RG:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
        channelNumber = 2;
        break;
    case CL_RA:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_G;
        channelNumber = 2;
        break;
    case CL_RGB:
    case CL_sRGB:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
        channelNumber = 3;
        break;
    case CL_RGBA:
    case CL_sRGBA:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
        channelNumber = 4;
        break;
    case CL_BGRA:
    case CL_sBGRA:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_B;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
        channelNumber = 4;
        break;
    case CL_ARGB:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_G;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_B;
        channelNumber = 4;
        break;
    case CL_ABGR:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_B;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_G;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
        channelNumber = 4;
        break;
    case CL_LUMINANCE:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
        channelNumber = 1;
        break;
    case CL_INTENSITY:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_R;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
        channelNumber = 1;
        break;
    case CL_DEPTH:
    case CL_DEPTH_STENCIL:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_D;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
        channelNumber = 1;
        break;
    default:
        l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32;
        l0Format.y = ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32;
        l0Format.z = ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32;
        l0Format.w = ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32;
        channelNumber = 1;
    }

    constexpr std::array<ze_image_format_layout_t, 4> layouts8 = {ZE_IMAGE_FORMAT_LAYOUT_8,
                                                                  ZE_IMAGE_FORMAT_LAYOUT_8_8,
                                                                  ZE_IMAGE_FORMAT_LAYOUT_8_8_8,
                                                                  ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8};
    constexpr std::array<ze_image_format_layout_t, 4> layouts16 = {ZE_IMAGE_FORMAT_LAYOUT_16,
                                                                   ZE_IMAGE_FORMAT_LAYOUT_16_16,
                                                                   ZE_IMAGE_FORMAT_LAYOUT_16_16_16,
                                                                   ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16};
    constexpr std::array<ze_image_format_layout_t, 4> layouts32 = {ZE_IMAGE_FORMAT_LAYOUT_32,
                                                                   ZE_IMAGE_FORMAT_LAYOUT_32_32,
                                                                   ZE_IMAGE_FORMAT_LAYOUT_32_32_32,
                                                                   ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32};

    switch (clChannelType) {
    case CL_UNSIGNED_INT8:
        l0Format.layout = layouts8[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
        break;
    case CL_SIGNED_INT8:
        l0Format.layout = layouts8[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_SINT;
        break;
    case CL_UNORM_INT8:
        l0Format.layout = layouts8[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_SNORM_INT8:
        l0Format.layout = layouts8[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_SNORM;
        break;
    case CL_UNSIGNED_INT16:
        l0Format.layout = layouts16[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
        break;
    case CL_SIGNED_INT16:
        l0Format.layout = layouts16[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_SINT;
        break;
    case CL_UNORM_INT16:
        l0Format.layout = layouts16[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_SNORM_INT16:
        l0Format.layout = layouts16[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_SNORM;
        break;
    case CL_HALF_FLOAT:
        l0Format.layout = layouts16[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_FLOAT;
        break;
    case CL_UNSIGNED_INT32:
        l0Format.layout = layouts32[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
        break;
    case CL_SIGNED_INT32:
        l0Format.layout = layouts32[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_SINT;
        break;
    case CL_FLOAT:
        l0Format.layout = layouts32[channelNumber - 1];
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_FLOAT;
        break;
    case CL_UNORM_INT_101010_2:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_UNORM_INT24:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_UNORM_SHORT_565:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_5_6_5;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_UNORM_SHORT_555:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_NV12_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_NV12;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_YUYV_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_YUYV;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_VYUY_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_VYUY;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_YVYU_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_YVYU;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    case CL_UYVY_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_UYVY;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
        break;
    default:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_FORCE_UINT32;
        l0Format.type = ZE_IMAGE_FORMAT_TYPE_FORCE_UINT32;
    }
}

const ClSurfaceFormatInfo *Image::getSurfaceFormatFromTable(cl_mem_flags flags, const cl_image_format *imageFormat) {
    ArrayRef<const ClSurfaceFormatInfo> formats = SurfaceFormats::surfaceFormats(flags, imageFormat);

    for (auto &format : formats) {
        if (format.oclImageFormat.image_channel_data_type == imageFormat->image_channel_data_type &&
            format.oclImageFormat.image_channel_order == imageFormat->image_channel_order) {
            return &format;
        }
    }
    return nullptr;
}

cl_int Image::getImageInfo(cl_image_info paramName,
                           size_t paramValueSize,
                           void *paramValue,
                           size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;
    size_t srcParamSize = GetInfo::invalidSourceSize;
    void *srcParam = nullptr;

    auto imgInfo = getL0Object()->getImageInfo();
    size_t arraySize = imgInfo.imgDesc.imageArraySize * (imgInfo.imgDesc.imageType == ImageType::image1DArray ||
                                                         imgInfo.imgDesc.imageType == ImageType::image2DArray);
    size_t slicePitch = imgInfo.slicePitch * !(imgInfo.imgDesc.imageType == ImageType::image2D ||
                                               imgInfo.imgDesc.imageType == ImageType::image1D ||
                                               imgInfo.imgDesc.imageType == ImageType::image1DBuffer);
    size_t elementSize = imgInfo.surfaceFormat->imageElementSizeInBytes;
    cl_uint numSamples = static_cast<cl_uint>(imgInfo.imgDesc.numSamples);
    cl_uint numMipLevels = static_cast<cl_uint>(imgInfo.imgDesc.numMipLevels);

    size_t imageWidth = imgInfo.imgDesc.imageWidth;
    if (imgInfo.baseMipLevel) {
        imageWidth >>= imgInfo.baseMipLevel;
        imageWidth = std::max(imageWidth, static_cast<size_t>(1));
    }

    size_t imageHeight = imgInfo.imgDesc.imageHeight * !((imgInfo.imgDesc.imageType == ImageType::image1D) ||
                                                         (imgInfo.imgDesc.imageType == ImageType::image1DArray) ||
                                                         (imgInfo.imgDesc.imageType == ImageType::image1DBuffer));

    if ((imageHeight != 0) && (imgInfo.baseMipLevel > 0)) {
        imageHeight >>= imgInfo.baseMipLevel;
        imageHeight = std::max(imageHeight, static_cast<size_t>(1));
    }

    size_t imageDepth = imgInfo.imgDesc.imageDepth * (imgInfo.imgDesc.imageType == ImageType::image3D);
    if ((imageDepth != 0) && (imgInfo.baseMipLevel > 0)) {
        imageDepth >>= imgInfo.baseMipLevel;
        imageDepth = std::max(imageDepth, static_cast<size_t>(1));
    }

    size_t rowPitch;
    if (numSamples > 1) {
        rowPitch = imgInfo.imgDesc.imageWidth * elementSize * numSamples;
    } else {
        rowPitch = imgInfo.rowPitch;
    }

    cl_mem bufferMem = static_cast<cl_mem>(this->associatedMemObject);

    switch (paramName) {
    case CL_IMAGE_FORMAT:
        srcParamSize = sizeof(cl_image_format);
        srcParam = &originalFormat;
        break;

    case CL_IMAGE_ELEMENT_SIZE:
        srcParamSize = sizeof(size_t);
        srcParam = &elementSize;
        break;

    case CL_IMAGE_ROW_PITCH:
        srcParamSize = sizeof(size_t);
        srcParam = &rowPitch;
        break;

    case CL_IMAGE_SLICE_PITCH:
        srcParamSize = sizeof(size_t);
        srcParam = &slicePitch;
        break;

    case CL_IMAGE_WIDTH:
        srcParamSize = sizeof(size_t);
        srcParam = &imageWidth;
        break;

    case CL_IMAGE_HEIGHT:
        srcParamSize = sizeof(size_t);
        srcParam = &imageHeight;
        break;

    case CL_IMAGE_DEPTH:
        srcParamSize = sizeof(size_t);
        srcParam = &imageDepth;
        break;

    case CL_IMAGE_ARRAY_SIZE:
        srcParamSize = sizeof(size_t);
        srcParam = &(arraySize);
        break;

    case CL_IMAGE_BUFFER:
        srcParamSize = sizeof(cl_mem);
        srcParam = &bufferMem;
        break;

    case CL_IMAGE_NUM_MIP_LEVELS:
        srcParamSize = sizeof(cl_uint);
        srcParam = &numMipLevels;
        break;

    case CL_IMAGE_NUM_SAMPLES:
        srcParamSize = sizeof(cl_uint);
        srcParam = &numSamples;
        break;

    default:
        getOsSpecificImageInfo(paramName, &srcParamSize, &srcParam);
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcParamSize, getInfoStatus);

    return retVal;
}

size_t Image::calculateTotalSizeForImage(const std::array<size_t, 3> &sizes) const {
    auto l0Image = getL0Object();
    const auto &l0ImgInfo = l0Image->getImageInfo();
    auto retSize = l0ImgInfo.surfaceFormat->imageElementSizeInBytes * sizes[0];
    auto rowPitch = l0ImgInfo.rowPitch;
    auto slicePitch = l0ImgInfo.slicePitch;

    switch (l0ImgInfo.imgDesc.imageType) {
    case NEO::ImageType::image1DArray:
        retSize += slicePitch * sizes[1];
        break;
    case NEO::ImageType::image2D:
        retSize += rowPitch * sizes[1];
        break;
    case NEO::ImageType::image2DArray:
    case NEO::ImageType::image3D:
        retSize += rowPitch * sizes[1] + slicePitch * sizes[2];
        break;
    default:
        break;
    }
    return retSize;
}

cl_mem_object_type Image::getClObjectType() {
    switch (getL0Object()->getImageInfo().imgDesc.imageType) {
    case ImageType::image1D:
        return CL_MEM_OBJECT_IMAGE1D;
    case ImageType::image1DArray:
        return CL_MEM_OBJECT_IMAGE1D_ARRAY;
    case ImageType::image1DBuffer:
        return CL_MEM_OBJECT_IMAGE1D_BUFFER;
    case ImageType::image2D:
        return CL_MEM_OBJECT_IMAGE2D;
    case ImageType::image2DArray:
        return CL_MEM_OBJECT_IMAGE2D_ARRAY;
    case ImageType::image3D:
        return CL_MEM_OBJECT_IMAGE3D;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

size_t Image::getApiSize() const {
    auto l0Image = getL0Object();
    const auto &l0ImgInfo = l0Image->getImageInfo();
    MemObjSizeArray sizes{l0ImgInfo.imgDesc.imageWidth,
                          l0ImgInfo.imgDesc.imageHeight,
                          l0ImgInfo.imgDesc.imageDepth};
    return this->calculateTotalSizeForImage(sizes);
}

size_t Image::getHostptrSize() const {
    auto l0Image = getL0Object();
    const auto &l0ImgInfo = l0Image->getImageInfo();
    MemObjSizeArray sizes{l0ImgInfo.imgDesc.imageWidth,
                          l0ImgInfo.imgDesc.imageType == ImageType::image1DArray ? l0ImgInfo.imgDesc.imageArraySize : l0ImgInfo.imgDesc.imageHeight,
                          l0ImgInfo.imgDesc.imageType == ImageType::image2DArray ? l0ImgInfo.imgDesc.imageArraySize : l0ImgInfo.imgDesc.imageDepth};
    return this->calculateTotalSizeForImage(sizes);
}

bool Image::isCompressionEnabled() {
    return false;
}

GraphicsAllocation *Image::getGraphicsAllocation(uint32_t rootDeviceIndex) { return static_cast<GraphicsAllocation *>(getL0Object(rootDeviceIndex)->getAllocation()); };

void Image::migrateTo(ze_command_list_handle_t cmdList, uint32_t targetRootDeviceIndex, bool outOfOrder,
                      uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (this->perDeviceImageHandles.empty() || this->associatedMemObject != nullptr) {
        return;
    }
    if (this->currentOwnerRootDeviceIndex == targetRootDeviceIndex) {
        return;
    }

    auto ownerHandle = this->getL0Handle(this->currentOwnerRootDeviceIndex);
    auto targetHandle = this->getL0Handle(targetRootDeviceIndex);
    if (ownerHandle == targetHandle) {
        return;
    }

    auto ret = zeCommandListAppendImageCopy(cmdList, targetHandle, ownerHandle, nullptr, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_SUCCESS) {
        return;
    }

    if (outOfOrder) {
        zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr);
    }

    this->currentOwnerRootDeviceIndex = targetRootDeviceIndex;
}

} // namespace LEO
} // namespace NEO
