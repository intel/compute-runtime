/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "leo_unified_image.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"

namespace NEO {
namespace LEO {

Image *UnifiedImage::createSharedUnifiedImage(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description,
                                              const cl_image_format *imageFormat, const cl_image_desc *imageDesc, cl_int *errcodeRet) {
    union {
        ze_external_memory_import_fd_t linuxImport;
        ze_external_memory_import_win32_handle_t windowsImport;
    } handleImport{};

    if (description.type == UnifiedSharingHandleType::linuxFd) {
        handleImport.linuxImport.fd = toOsHandle(description.handle);
        handleImport.linuxImport.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
        handleImport.linuxImport.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    } else {
        handleImport.windowsImport.handle = description.handle;
        handleImport.windowsImport.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
        handleImport.windowsImport.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    }

    ze_image_desc_t l0imageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};

    l0imageDesc.miplevels = imageDesc->num_mip_levels;
    l0imageDesc.width = static_cast<uint32_t>(imageDesc->image_width);
    l0imageDesc.height = static_cast<uint32_t>(imageDesc->image_height ? imageDesc->image_height : 1u);
    l0imageDesc.depth = static_cast<uint32_t>(imageDesc->image_depth ? imageDesc->image_depth : 1u);
    l0imageDesc.arraylevels = static_cast<uint32_t>(imageDesc->image_array_size);
    l0imageDesc.pNext = &handleImport;

    l0imageDesc.type = Image::clToL0ImageType(imageDesc->image_type);
    Image::clToL0ImageFormat(l0imageDesc.format, imageFormat->image_channel_order, imageFormat->image_channel_data_type);

    ze_srgb_ext_desc_t srgbExtDesc{ZE_STRUCTURE_TYPE_SRGB_EXT_DESC, l0imageDesc.pNext, Image::isSRGB(imageFormat->image_channel_order)};
    l0imageDesc.pNext = &srgbExtDesc;

    ze_image_handle_t imageHandle{};
    ze_result_t ret = zeImageCreate(context->getL0ContextHandle(),
                                    context->getL0Object()->getDevices().begin()->second,
                                    &l0imageDesc,
                                    &imageHandle);

    if (ret != ZE_RESULT_SUCCESS) {
        return nullptr;
    }

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto image = new Image(context, memoryProperties, flags, imageHandle, nullptr, nullptr, false, *imageFormat, nullptr);

    auto sharingHandler = new UnifiedImage(context->getSharing<UnifiedSharingFunctions>(), description.type);
    image->setSharingHandler(sharingHandler);

    return image;
}

} // namespace LEO
} // namespace NEO
