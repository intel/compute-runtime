/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/d3d/d3d_texture.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/helpers/gmm_types_converter.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/mem_obj/image.h"

namespace NEO {
namespace LEO {

std::pair<cl_channel_order, cl_channel_type>
dxgiToOpenCLImageFormat(DXGI_FORMAT dxgiFormat) {
    switch (dxgiFormat) {
    case DXGI_FORMAT_R8_UNORM:
        return {CL_R, CL_UNORM_INT8};
    case DXGI_FORMAT_R8_SNORM:
        return {CL_R, CL_SNORM_INT8};
    case DXGI_FORMAT_R8_UINT:
        return {CL_R, CL_UNSIGNED_INT8};
    case DXGI_FORMAT_R8_SINT:
        return {CL_R, CL_SIGNED_INT8};

    case DXGI_FORMAT_R8G8_UNORM:
        return {CL_RG, CL_UNORM_INT8};
    case DXGI_FORMAT_R8G8_SNORM:
        return {CL_RG, CL_SNORM_INT8};
    case DXGI_FORMAT_R8G8_UINT:
        return {CL_RG, CL_UNSIGNED_INT8};
    case DXGI_FORMAT_R8G8_SINT:
        return {CL_RG, CL_SIGNED_INT8};

    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return {CL_RGBA, CL_UNORM_INT8};
    case DXGI_FORMAT_R8G8B8A8_SNORM:
        return {CL_RGBA, CL_SNORM_INT8};
    case DXGI_FORMAT_R8G8B8A8_UINT:
        return {CL_RGBA, CL_UNSIGNED_INT8};
    case DXGI_FORMAT_R8G8B8A8_SINT:
        return {CL_RGBA, CL_SIGNED_INT8};

    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return {CL_BGRA, CL_UNORM_INT8};

    case DXGI_FORMAT_R16_UNORM:
        return {CL_R, CL_UNORM_INT16};
    case DXGI_FORMAT_R16_SNORM:
        return {CL_R, CL_SNORM_INT16};
    case DXGI_FORMAT_R16_UINT:
        return {CL_R, CL_UNSIGNED_INT16};
    case DXGI_FORMAT_R16_SINT:
        return {CL_R, CL_SIGNED_INT16};
    case DXGI_FORMAT_R16_FLOAT:
        return {CL_R, CL_HALF_FLOAT};

    case DXGI_FORMAT_R16G16_UNORM:
        return {CL_RG, CL_UNORM_INT16};
    case DXGI_FORMAT_R16G16_SNORM:
        return {CL_RG, CL_SNORM_INT16};
    case DXGI_FORMAT_R16G16_UINT:
        return {CL_RG, CL_UNSIGNED_INT16};
    case DXGI_FORMAT_R16G16_SINT:
        return {CL_RG, CL_SIGNED_INT16};
    case DXGI_FORMAT_R16G16_FLOAT:
        return {CL_RG, CL_HALF_FLOAT};

    case DXGI_FORMAT_R16G16B16A16_UNORM:
        return {CL_RGBA, CL_UNORM_INT16};
    case DXGI_FORMAT_R16G16B16A16_SNORM:
        return {CL_RGBA, CL_SNORM_INT16};
    case DXGI_FORMAT_R16G16B16A16_UINT:
        return {CL_RGBA, CL_UNSIGNED_INT16};
    case DXGI_FORMAT_R16G16B16A16_SINT:
        return {CL_RGBA, CL_SIGNED_INT16};
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return {CL_RGBA, CL_HALF_FLOAT};

    case DXGI_FORMAT_R32_FLOAT:
        return {CL_R, CL_FLOAT};
    case DXGI_FORMAT_R32_UINT:
        return {CL_R, CL_UNSIGNED_INT32};
    case DXGI_FORMAT_R32_SINT:
        return {CL_R, CL_SIGNED_INT32};

    case DXGI_FORMAT_R32G32_FLOAT:
        return {CL_RG, CL_FLOAT};
    case DXGI_FORMAT_R32G32_UINT:
        return {CL_RG, CL_UNSIGNED_INT32};
    case DXGI_FORMAT_R32G32_SINT:
        return {CL_RG, CL_SIGNED_INT32};

    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return {CL_RGBA, CL_FLOAT};
    case DXGI_FORMAT_R32G32B32A32_UINT:
        return {CL_RGBA, CL_UNSIGNED_INT32};
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return {CL_RGBA, CL_SIGNED_INT32};

    default:
        return {0, 0};
    }
}

template <typename D3D>
Image *D3DTexture<D3D>::create2d(Context *context, D3DTexture2d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    ImagePlane imagePlane = ImagePlane::noPlane;
    void *sharedHandle = nullptr;
    cl_uint arrayIndex = 0u;

    D3DTexture2dDesc textureDesc = {};
    sharingFcns->getTexture2dDesc(&textureDesc, d3dTexture);

    cl_int formatSupportError = sharingFcns->validateFormatSupport(textureDesc.Format, CL_MEM_OBJECT_IMAGE2D);
    if (formatSupportError != CL_SUCCESS) {
        err.set(formatSupportError);
        return nullptr;
    }

    bool needsView = D3DSharing<D3D>::isFormatWithPlane1(textureDesc.Format);

    if (needsView) {
        if ((subresource % 2) == 0) {
            imagePlane = ImagePlane::planeY;
        } else {
            imagePlane = ImagePlane::planeUV;
        }
        arrayIndex = subresource / 2u;
    } else if (subresource >= textureDesc.MipLevels * textureDesc.ArraySize) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    bool sharedResource = false;
    D3DTexture2d *textureStaging = nullptr;
    if ((textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED ||
         textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_KEYEDMUTEX) &&
        subresource % textureDesc.MipLevels == 0) {
        textureStaging = d3dTexture;
        sharedResource = true;
    } else {
        sharingFcns->createTexture2d(&textureStaging, &textureDesc, subresource);
    }

    bool ntHandle = textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE;
    ntHandle ? sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle) : sharingFcns->getSharedHandle(textureStaging, &sharedHandle);

    ze_external_memory_import_win32_handle_t l0imageHandleDesc{ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32};
    l0imageHandleDesc.handle = sharedHandle;
    l0imageHandleDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

    ze_image_desc_t l0imageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};
    l0imageDesc.miplevels = textureDesc.MipLevels;
    l0imageDesc.pNext = &l0imageHandleDesc;
    l0imageDesc.type = ze_image_type_t::ZE_IMAGE_TYPE_2D;
    l0imageDesc.width = textureDesc.Width;
    l0imageDesc.height = textureDesc.Height;
    l0imageDesc.depth = 1;
    l0imageDesc.arraylevels = arrayIndex;

    if (needsView) {
        switch (textureDesc.Format) {
        case DXGI_FORMAT_NV12:
            l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_NV12;
            break;
        case DXGI_FORMAT_P010:
            l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_P010;
            break;
        case DXGI_FORMAT_P016:
            l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_P016;
            break;
        case DXGI_FORMAT_420_OPAQUE:
            l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_I420;
            break;
        default:
            UNRECOVERABLE_IF(true);
            break;
        }
    }

    ze_result_t ret = ZE_RESULT_SUCCESS;
    ze_image_handle_t baseImageHandle{};
    ze_image_view_planar_ext_desc_t l0imagePlannarDesc{ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXT_DESC};

    if (needsView) {
        ret = zeImageCreate(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, &baseImageHandle);

        if (ret != ZE_RESULT_SUCCESS) {
            err.set(L0ToClResultMapper(ret));
            return nullptr;
        }

        l0imagePlannarDesc.planeIndex = static_cast<int>(GmmTypesConverter::convertPlane(imagePlane)) - 1;
        l0imageDesc.pNext = &l0imagePlannarDesc;
    }

    auto [clChannelOrder, clChannelType] = dxgiToOpenCLImageFormat(textureDesc.Format);
    Image::clToL0ImageFormat(l0imageDesc.format, clChannelOrder, clChannelType);

    ze_srgb_ext_desc_t srgbExtDesc{ZE_STRUCTURE_TYPE_SRGB_EXT_DESC, l0imageDesc.pNext, Image::isSRGB(clChannelOrder)};
    l0imageDesc.pNext = &srgbExtDesc;

    ze_image_handle_t imageHandle{};

    if (needsView) {
        if (imagePlane == ImagePlane::planeU || imagePlane == ImagePlane::planeV || imagePlane == ImagePlane::planeUV) {
            l0imageDesc.width /= 2;
            l0imageDesc.height /= 2;
        }
        ret = zeImageViewCreateExp(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, baseImageHandle, &imageHandle);
    } else {
        ret = zeImageCreate(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, &imageHandle);
    }

    if (ret != ZE_RESULT_SUCCESS) {
        err.set(L0ToClResultMapper(ret));
        return nullptr;
    }

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto image = new Image(context, memoryProperties, flags, imageHandle, nullptr, baseImageHandle, false, cl_image_format{clChannelOrder, clChannelType}, nullptr);

    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);
    image->setSharingHandler(d3dTextureObj);

    return image;
}

template <typename D3D>
Image *D3DTexture<D3D>::create3d(Context *context, D3DTexture3d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    void *sharedHandle = nullptr;

    D3DTexture3dDesc textureDesc = {};
    sharingFcns->getTexture3dDesc(&textureDesc, d3dTexture);

    cl_int formatSupportError = sharingFcns->validateFormatSupport(textureDesc.Format, CL_MEM_OBJECT_IMAGE2D);
    if (formatSupportError != CL_SUCCESS) {
        err.set(formatSupportError);
        return nullptr;
    }

    if (subresource >= textureDesc.MipLevels) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    bool sharedResource = false;
    D3DTexture3d *textureStaging = nullptr;
    if ((textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED || textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_KEYEDMUTEX) &&
        subresource == 0) {
        textureStaging = d3dTexture;
        sharedResource = true;
    } else {
        sharingFcns->createTexture3d(&textureStaging, &textureDesc, subresource);
    }

    bool ntHandle = textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE;
    ntHandle ? sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle) : sharingFcns->getSharedHandle(textureStaging, &sharedHandle);

    ze_external_memory_import_win32_handle_t l0imageHandleDesc{ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32};
    l0imageHandleDesc.handle = sharedHandle;
    l0imageHandleDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

    ze_image_desc_t l0imageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};
    l0imageDesc.miplevels = 0;
    l0imageDesc.pNext = &l0imageHandleDesc;
    l0imageDesc.type = ze_image_type_t::ZE_IMAGE_TYPE_3D;
    l0imageDesc.width = textureDesc.Width;
    l0imageDesc.height = textureDesc.Height;
    l0imageDesc.depth = textureDesc.Depth;
    l0imageDesc.arraylevels = 0;

    auto [clChannelOrder, clChannelType] = dxgiToOpenCLImageFormat(textureDesc.Format);
    Image::clToL0ImageFormat(l0imageDesc.format, clChannelOrder, clChannelType);

    ze_srgb_ext_desc_t srgbExtDesc{ZE_STRUCTURE_TYPE_SRGB_EXT_DESC, l0imageDesc.pNext, Image::isSRGB(clChannelOrder)};
    l0imageDesc.pNext = &srgbExtDesc;

    ze_image_handle_t imageHandle{};
    auto ret = zeImageCreate(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, &imageHandle);

    if (ret != ZE_RESULT_SUCCESS) {
        err.set(L0ToClResultMapper(ret));
        return nullptr;
    }

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto image = new Image(context, memoryProperties, flags, imageHandle, nullptr, nullptr, false, cl_image_format{clChannelOrder, clChannelType}, nullptr);

    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);
    image->setSharingHandler(d3dTextureObj);

    return image;
}

template class D3DTexture<D3DTypesHelper::D3D10>;
template class D3DTexture<D3DTypesHelper::D3D11>;

} // namespace LEO
} // namespace NEO
