/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/mem_obj/leo_image.h"

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/image_enqueue_helpers.h"
#include "level_zero/api/opencl/source/helpers/leo_convert_color.h"
#include "level_zero/api/opencl/source/helpers/leo_get_info_status_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_surface_formats.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"
#include "level_zero/core/source/kernel/kernel.h"
#include <level_zero/ze_api.h>

#include "CL/cl_gl.h"

#include <algorithm>
#include <cstring>

namespace NEO {
namespace LEO {

Image::Image(Context *context, MemoryProperties &properties, cl_mem_flags flags, ze_image_handle_t imageHandle,
             void *cpuPtr, ze_image_handle_t baseImageHandle, bool externalHandle, cl_image_format originalFormat,
             cl_mem memObject)
    : MemObj(context, properties, flags, cpuPtr, externalHandle, MemObjType::image),
      imageHandle(imageHandle), baseImageHandle(baseImageHandle), originalFormat(originalFormat) {
    magic = objectMagic;
    this->associatedMemObject = memObject ? castToObject<MemObj>(memObject) : nullptr;
    if (this->associatedMemObject) {
        // Image borrows the parent's storage, so the parent must outlive it.
        this->associatedMemObject->incRefInternal();
        this->setParentSharingHandler(this->associatedMemObject->getSharingHandler());
    }
}

Image::~Image() {
    if (!externalHandle && this->imageHandle) {
        UNRECOVERABLE_IF(zeImageDestroy(this->imageHandle) != ZE_RESULT_SUCCESS);
    }
    if (!externalHandle) {
        for (auto &[rootDeviceIndex, handle] : this->perDeviceImageHandles) {
            UNRECOVERABLE_IF(zeImageDestroy(handle) != ZE_RESULT_SUCCESS);
        }
    }
    if (this->baseImageHandle) {
        UNRECOVERABLE_IF(zeImageDestroy(this->baseImageHandle) != ZE_RESULT_SUCCESS);
    }
    // Must follow zeImageDestroy above - the parent owns the storage backing this image.
    if (this->associatedMemObject) {
        this->associatedMemObject->decRefInternal();
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
    // Media layouts ignore the format type and the swizzles, the layout alone describes them.
    case CL_NV12_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_NV12;
        return;
    case CL_YUYV_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_YUYV;
        return;
    case CL_VYUY_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_VYUY;
        return;
    case CL_YVYU_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_YVYU;
        return;
    case CL_UYVY_INTEL:
        l0Format.layout = ZE_IMAGE_FORMAT_LAYOUT_UYVY;
        return;

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

size_t Image::getRowPitchForImageFromBuffer(cl_mem_flags flags, const cl_image_format *imageFormat, const cl_image_desc *imageDesc) {
    // Per the OpenCL spec, when image_row_pitch is 0 it is calculated as
    // image_width * size of element in bytes.
    if (imageDesc->mem_object == nullptr || imageDesc->image_row_pitch != 0) {
        return imageDesc->image_row_pitch;
    }
    auto surfaceFormat = getSurfaceFormatFromTable(flags, imageFormat);
    if (surfaceFormat == nullptr) {
        return imageDesc->image_row_pitch;
    }
    return imageDesc->image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
}

cl_image_desc Image::getClImageDesc() {
    const auto &imgInfo = getL0Object()->getImageInfo();
    const auto imageType = imgInfo.imgDesc.imageType;
    auto getBaseMipLevelDimension = [&imgInfo](size_t dimension) {
        if ((dimension == 0) || (imgInfo.baseMipLevel == 0)) {
            return dimension;
        }
        return std::max(dimension >> imgInfo.baseMipLevel, static_cast<size_t>(1));
    };

    cl_image_desc clImageDesc{};
    clImageDesc.image_type = this->getClObjectType();
    clImageDesc.image_width = getBaseMipLevelDimension(imgInfo.imgDesc.imageWidth);
    clImageDesc.image_height = getBaseMipLevelDimension(imgInfo.imgDesc.imageHeight * !((imageType == ImageType::image1D) ||
                                                                                        (imageType == ImageType::image1DArray) ||
                                                                                        (imageType == ImageType::image1DBuffer)));
    clImageDesc.image_depth = getBaseMipLevelDimension(imgInfo.imgDesc.imageDepth * (imageType == ImageType::image3D));
    clImageDesc.image_array_size = imgInfo.imgDesc.imageArraySize * ((imageType == ImageType::image1DArray) ||
                                                                     (imageType == ImageType::image2DArray));
    clImageDesc.num_mip_levels = static_cast<cl_uint>(imgInfo.imgDesc.numMipLevels);
    clImageDesc.num_samples = static_cast<cl_uint>(imgInfo.imgDesc.numSamples);
    if (clImageDesc.num_samples > 1) {
        clImageDesc.image_row_pitch = imgInfo.imgDesc.imageWidth * imgInfo.surfaceFormat->imageElementSizeInBytes * clImageDesc.num_samples;
    } else {
        clImageDesc.image_row_pitch = this->getHostRowPitch();
    }
    clImageDesc.image_slice_pitch = this->getHostSlicePitch();
    clImageDesc.mem_object = static_cast<cl_mem>(this->associatedMemObject);
    return clImageDesc;
}

cl_int Image::getImageInfo(cl_image_info paramName,
                           size_t paramValueSize,
                           void *paramValue,
                           size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;
    size_t srcParamSize = GetInfo::invalidSourceSize;
    void *srcParam = nullptr;

    auto clImageDesc = this->getClImageDesc();
    size_t elementSize = getL0Object()->getImageInfo().surfaceFormat->imageElementSizeInBytes;
    std::vector<ze_image_handle_t> l0Handles{};

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
        srcParam = &clImageDesc.image_row_pitch;
        break;

    case CL_IMAGE_SLICE_PITCH:
        srcParamSize = sizeof(size_t);
        srcParam = &clImageDesc.image_slice_pitch;
        break;

    case CL_IMAGE_WIDTH:
        srcParamSize = sizeof(size_t);
        srcParam = &clImageDesc.image_width;
        break;

    case CL_IMAGE_HEIGHT:
        srcParamSize = sizeof(size_t);
        srcParam = &clImageDesc.image_height;
        break;

    case CL_IMAGE_DEPTH:
        srcParamSize = sizeof(size_t);
        srcParam = &clImageDesc.image_depth;
        break;

    case CL_IMAGE_ARRAY_SIZE:
        srcParamSize = sizeof(size_t);
        srcParam = &clImageDesc.image_array_size;
        break;

    case CL_IMAGE_BUFFER:
        srcParamSize = sizeof(cl_mem);
        srcParam = &clImageDesc.mem_object;
        break;

    case CL_IMAGE_NUM_MIP_LEVELS:
        srcParamSize = sizeof(cl_uint);
        srcParam = &clImageDesc.num_mip_levels;
        break;

    case CL_IMAGE_NUM_SAMPLES:
        srcParamSize = sizeof(cl_uint);
        srcParam = &clImageDesc.num_samples;
        break;

    case CL_IMAGE_L0_HANDLE_INTEL:
        l0Handles = this->getL0Handles();
        srcParamSize = l0Handles.size() * sizeof(ze_image_handle_t);
        srcParam = l0Handles.data();
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

size_t Image::calculateHostPtrSizeForImage(const std::array<size_t, 3> &sizes, const MipLevelHostLayout &layout) const {
    const size_t precedingRows = sizes[1] > 0u ? sizes[1] - 1u : 0u;
    const size_t precedingSlices = sizes[2] > 0u ? sizes[2] - 1u : 0u;
    return this->calculateHostPtrOffsetInMipLevel({sizes[0], precedingRows, precedingSlices}, layout);
}

size_t Image::calculateHostPtrOffsetForImage(const std::array<size_t, 3> &origin, const MipLevelHostLayout &layout) const {
    return layout.offset + this->calculateHostPtrOffsetInMipLevel(origin, layout);
}

size_t Image::calculateHostPtrOffsetInMipLevel(const std::array<size_t, 3> &origin, const MipLevelHostLayout &layout) const {
    const auto &l0ImgInfo = getL0Object()->getImageInfo();
    auto offset = l0ImgInfo.surfaceFormat->imageElementSizeInBytes * origin[0];

    switch (l0ImgInfo.imgDesc.imageType) {
    case NEO::ImageType::image1DArray:
        offset += layout.slicePitch * origin[1];
        break;
    case NEO::ImageType::image2D:
        offset += layout.rowPitch * origin[1];
        break;
    case NEO::ImageType::image2DArray:
    case NEO::ImageType::image3D:
        offset += layout.rowPitch * origin[1] + layout.slicePitch * origin[2];
        break;
    default:
        break;
    }
    return offset;
}

std::array<size_t, 3> Image::getMipLevelHostSizes(uint32_t mipLevel) const {
    const auto &imgDesc = getL0Object()->getImageInfo().imgDesc;
    auto getMipLevelDimension = [mipLevel](size_t baseDimension) {
        return std::max<size_t>(baseDimension >> mipLevel, 1u);
    };

    switch (imgDesc.imageType) {
    case NEO::ImageType::image1DArray:
        return {getMipLevelDimension(imgDesc.imageWidth), imgDesc.imageArraySize, 1u};
    case NEO::ImageType::image2DArray:
        return {getMipLevelDimension(imgDesc.imageWidth), getMipLevelDimension(imgDesc.imageHeight), imgDesc.imageArraySize};
    default:
        return {getMipLevelDimension(imgDesc.imageWidth), getMipLevelDimension(imgDesc.imageHeight), getMipLevelDimension(imgDesc.imageDepth)};
    }
}

MipLevelHostLayout Image::getMipLevelHostLayout(uint32_t mipLevel) const {
    const auto &l0ImgInfo = getL0Object()->getImageInfo();
    MipLevelHostLayout layout{0u, this->getHostRowPitch(), this->getHostSlicePitch()};

    for (uint32_t level = 1; level <= mipLevel; level++) {
        layout.offset += this->calculateHostPtrSizeForImage(this->getMipLevelHostSizes(level - 1), layout);

        const auto levelSizes = this->getMipLevelHostSizes(level);
        layout.rowPitch = levelSizes[0] * l0ImgInfo.surfaceFormat->imageElementSizeInBytes;
        switch (l0ImgInfo.imgDesc.imageType) {
        case NEO::ImageType::image1DArray:
            layout.slicePitch = layout.rowPitch;
            break;
        case NEO::ImageType::image2DArray:
        case NEO::ImageType::image3D:
            layout.slicePitch = layout.rowPitch * levelSizes[1];
            break;
        default:
            layout.slicePitch = 0u;
            break;
        }
    }
    return layout;
}

size_t Image::getHostRowPitch() const {
    if (this->hostPtrRowPitch != 0) {
        return this->hostPtrRowPitch;
    }
    const auto &l0ImgInfo = getL0Object()->getImageInfo();
    return l0ImgInfo.imgDesc.imageWidth * l0ImgInfo.surfaceFormat->imageElementSizeInBytes;
}

size_t Image::getHostSlicePitch() const {
    const auto imageType = getL0Object()->getImageInfo().imgDesc.imageType;
    switch (imageType) {
    case NEO::ImageType::image1DArray:
        return this->hostPtrSlicePitch != 0 ? this->hostPtrSlicePitch : this->getHostRowPitch();
    case NEO::ImageType::image2DArray:
    case NEO::ImageType::image3D:
        return this->hostPtrSlicePitch != 0 ? this->hostPtrSlicePitch : this->getHostRowPitch() * getL0Object()->getImageInfo().imgDesc.imageHeight;
    default:
        return 0u;
    }
}

size_t Image::getHostptrSize() const {
    const auto numMipLevels = getL0Object()->getImageInfo().imgDesc.numMipLevels;
    const uint32_t lastMipLevel = std::max(numMipLevels, 1u) - 1u;
    const auto lastMipLevelLayout = this->getMipLevelHostLayout(lastMipLevel);
    return lastMipLevelLayout.offset + this->calculateHostPtrSizeForImage(this->getMipLevelHostSizes(lastMipLevel), lastMipLevelLayout);
}

bool Image::isCompressionEnabled() {
    return false;
}

GraphicsAllocation *Image::getGraphicsAllocation(uint32_t rootDeviceIndex) { return static_cast<GraphicsAllocation *>(getL0Object(rootDeviceIndex)->getAllocation()); };

std::vector<ze_image_handle_t> Image::getL0Handles() const {
    const auto &rootDeviceIndices = this->context->getRootDeviceIndices();
    std::vector<ze_image_handle_t> handles{};
    handles.reserve(rootDeviceIndices.size());
    for (const auto &rootDeviceIndex : rootDeviceIndices) {
        handles.push_back(this->getL0Handle(rootDeviceIndex));
    }
    return handles;
}

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

static void packFillColor(const ImageInfo &imgInfo, const cl_image_format &originalFormat, const void *fillColor, uint8_t (&packedFillColor)[16]) {
    int32_t convertedFillColor[4] = {0};
    NEO::convertFillColor(fillColor, convertedFillColor, originalFormat, NEO::redescribeFillColor(imgInfo));

    auto numChannels = imgInfo.surfaceFormat->numChannels;
    auto perChannelSize = imgInfo.surfaceFormat->perChannelSizeInBytes;
    for (uint32_t ch = 0; ch < numChannels; ch++) {
        memcpy(&packedFillColor[ch * perChannelSize], &convertedFillColor[ch], perChannelSize);
    }
}

ze_result_t Image::appendFill(ze_command_list_handle_t cmdList, ClDevice *device, const void *fillColor, const size_t *origin, const size_t *region,
                              ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    const auto rootDeviceIndex = device->getRootDeviceIndex();

    if (this->getClObjectType() == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        auto l0ImageImp = this->getL0Object(rootDeviceIndex);
        auto imgInfo = l0ImageImp->getImageInfo();

        uint8_t packedFillColor[16] = {0};
        packFillColor(imgInfo, this->getOriginalFormat(), fillColor, packedFillColor);

        auto elementSize = imgInfo.surfaceFormat->imageElementSizeInBytes;
        auto dstPtr = reinterpret_cast<void *>(l0ImageImp->getAllocation()->getGpuAddress() + imgInfo.offset + origin[0] * elementSize);
        auto fillSize = region[0] * elementSize;

        return zeCommandListAppendMemoryFill(cmdList, dstPtr, packedFillColor, elementSize, fillSize, hSignalEvent, numWaitEvents, phWaitEvents);
    }

    auto fillRegion = createZeImageRegionWithMipLevel(this, origin, region);
    return appendFillWithBuiltin(cmdList, device->getL0Object(), this->getL0Handle(rootDeviceIndex), this->getOriginalFormat(), fillColor, fillRegion, fillRegion.mipLevel,
                                 hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t Image::appendFillWithBuiltin(ze_command_list_handle_t cmdList, L0::Device *l0Device, ze_image_handle_t l0Image, const cl_image_format &originalFormat,
                                         const void *fillColor, const ze_image_region_t &fillRegion, uint32_t mipLevel,
                                         ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    uint8_t packedFillColor[16] = {0};
    packFillColor(L0::Image::fromHandle(l0Image)->getImageInfo(), originalFormat, fillColor, packedFillColor);

    auto builtInMode = l0Device->getCompilerProductHelper().getDefaultBuiltInAddressingMode(
        NEO::ApiSpecificConfig::getBindlessMode(*l0Device->getNEODevice()));

    auto lock = l0Device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    auto *builtinKernel = l0Device->getBuiltinFunctionsLib()->getImageFunction(L0::ImageBuiltIn::fillImage3d, builtInMode);

    builtinKernel->setArgRedescribedImage(0u, l0Image, false, mipLevel);
    builtinKernel->setArgumentValue(1u, sizeof(packedFillColor), packedFillColor);

    uint32_t dstOffset[] = {fillRegion.originX, fillRegion.originY, fillRegion.originZ, 0u};
    builtinKernel->setArgumentValue(2u, sizeof(dstOffset), dstOffset);

    const uint32_t regionX = fillRegion.width;
    const uint32_t regionY = std::max(fillRegion.height, 1u);
    const uint32_t regionZ = std::max(fillRegion.depth, 1u);

    uint32_t groupSizeX = regionX;
    uint32_t groupSizeY = regionY;
    uint32_t groupSizeZ = regionZ;

    builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);
    builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);

    ze_group_count_t groupCount{regionX / groupSizeX, regionY / groupSizeY, regionZ / groupSizeZ};

    return zeCommandListAppendLaunchKernel(cmdList, builtinKernel->toHandle(), &groupCount, hSignalEvent, numWaitEvents, phWaitEvents);
}

} // namespace LEO
} // namespace NEO
