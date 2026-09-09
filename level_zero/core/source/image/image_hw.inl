/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/properties_parser.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_formats.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/image/image_imp.h"
#include "level_zero/core/source/sampler/sampler.h"

#include "encode_surface_state_args.h"

namespace L0 {

static_assert(ImageFormats::maxTypeCount == static_cast<uint32_t>(ZE_IMAGE_FORMAT_TYPE_FLOAT) + 1u);
static_assert(ImageFormats::maxLayoutCount == static_cast<uint32_t>(ZE_IMAGE_FORMAT_LAYOUT_32_32_32) + 1u);

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t ImageCoreFamily<gfxCoreFamily>::initialize(Device *device, const ze_image_desc_t *desc) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    StructuresLookupTable lookupTable = {};

    lookupTable.areImageProperties = true;
    lookupTable.imageProperties.imageDescriptor = convertDescriptor(*desc);

    auto parseResult = prepareL0StructuresLookupTable(lookupTable, desc->pNext);

    if (parseResult != ZE_RESULT_SUCCESS) {
        return parseResult;
    }

    const bool isMediaFormatLayout = isMediaFormat(desc->format.layout);

    const bool hasFixedChannelSelect = isMediaFormatLayout || isPackedYuvFormat(desc->format.layout);

    if (!hasFixedChannelSelect && !hasValidSwizzles(desc->format)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;
    }

    imgInfo.imgDesc = lookupTable.imageProperties.imageDescriptor;
    imgInfo.mipCount = imgInfo.imgDesc.numMipLevels > 1 ? imgInfo.imgDesc.numMipLevels : 0;
    if (imgInfo.imgDesc.numMipLevels > 1) {
        mipLevelBindlessInfos = std::vector<std::unique_ptr<NEO::SurfaceStateInHeapInfo>>(imgInfo.imgDesc.numMipLevels);
    }

    if (lookupTable.isSrgb) {
        if (desc->format.layout != ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8 || desc->format.type != ZE_IMAGE_FORMAT_TYPE_UNORM) {
            return ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;
        }
        imgInfo.surfaceFormat = &ImageFormats::srgbFormatRGBA8;
        this->srgbImage = true;
    } else if (lookupTable.isDepthStencilFormat) {
        if (lookupTable.depthStencilFormat == ZE_DEPTH_STENCIL_FORMAT_D32_FLOAT_S8X24_UINT) {
            imgInfo.surfaceFormat = &ImageFormats::depthStencilFormatD32FS8;
        } else {
            imgInfo.surfaceFormat = &ImageFormats::depthStencilFormatD24S8;
        }
        this->depthStencilImage = true;
    } else {
        if (static_cast<uint32_t>(desc->format.layout) >= ImageFormats::maxLayoutCount) {
            return ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;
        }
        const auto formatType = hasIgnoredFormatType(desc->format.layout) ? ZE_IMAGE_FORMAT_TYPE_UNORM : desc->format.type;
        if (static_cast<uint32_t>(formatType) >= ImageFormats::maxTypeCount) {
            return ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;
        }
        imgInfo.surfaceFormat = &ImageFormats::formats[desc->format.layout][formatType];
    }
    imageFormatDesc = *const_cast<ze_image_desc_t *>(desc);

    UNRECOVERABLE_IF(device == nullptr);
    this->device = device;

    if (imgInfo.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_INVALID) {
        return ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;
    }

    typename RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType;
    switch (desc->type) {
    case ZE_IMAGE_TYPE_BUFFER:
    case ZE_IMAGE_TYPE_1D:
    case ZE_IMAGE_TYPE_1DARRAY:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
        break;
    case ZE_IMAGE_TYPE_2D:
    case ZE_IMAGE_TYPE_2DARRAY:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D;
        break;
    case ZE_IMAGE_TYPE_3D:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D;
        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    imgInfo.linearStorage = surfaceType == RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
    imgInfo.plane = lookupTable.imageProperties.isPlanarExtension ? static_cast<NEO::ImagePlane>(lookupTable.imageProperties.planeIndex + 1u) : NEO::ImagePlane::noPlane;
    imgInfo.useLocalMemory = false;

    if (lookupTable.bindlessImage && this->device->getNEODevice()->getBindlessHeapsHelper() == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    this->bindlessImage = lookupTable.bindlessImage;

    if (lookupTable.imageProperties.pitchedPtr) {
        if (isImageView() || (!this->bindlessImage && desc->type != ZE_IMAGE_TYPE_BUFFER && desc->type != ZE_IMAGE_TYPE_2D)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        this->imageFromBuffer = true;
    } else if (desc->type == ZE_IMAGE_TYPE_BUFFER) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (lookupTable.sampledImage) {
        if (!this->bindlessImage || lookupTable.imageProperties.samplerDesc == nullptr) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        this->sampledImage = true;
        this->samplerDesc = *lookupTable.imageProperties.samplerDesc;
        this->samplerDesc.pNext = nullptr;
    }

    if (!isImageView()) {
        if (lookupTable.isSharedHandle) {
            if (!lookupTable.sharedHandleType.isSupportedHandle) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
            if (lookupTable.sharedHandleType.isOpaqueFDHandle || lookupTable.sharedHandleType.isDMABUFHandle) {
                if (lookupTable.imageTilingOverride.present) {
                    imgInfo.linearStorage = lookupTable.imageTilingOverride.linearStorage;
                    if (!lookupTable.imageTilingOverride.linearStorage) {
                        imgInfo.forceTiling = lookupTable.imageTilingOverride.forceTiling;
                    }
                }
                NEO::MemoryManager::OsHandleData osHandleData{static_cast<NEO::osHandle>(lookupTable.sharedHandleType.fd)};
                NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, &imgInfo, NEO::AllocationType::sharedImage, device->getNEODevice()->getDeviceBitfield());
                allocation = device->getNEODevice()->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
                device->getNEODevice()->getMemoryManager()->closeSharedHandle(allocation);
            } else if (lookupTable.sharedHandleType.isNTHandle) {
                const uint32_t importArrayIndex = lookupTable.d3dTextureExt.present
                                                      ? lookupTable.d3dTextureExt.arrayIndex
                                                      : 0u;
                NEO::MemoryManager::OsHandleData osHandleData{lookupTable.sharedHandleType.ntHandle, importArrayIndex};
                NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, &imgInfo, NEO::AllocationType::sharedImage, device->getNEODevice()->getDeviceBitfield());
                allocation = device->getNEODevice()->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
                if (allocation != nullptr && lookupTable.glTextureExt.present) {
                    this->device = device;
                    if (lookupTable.glTextureExt.numberOfSamples > 1) {
                        imgInfo.imgDesc.numSamples = lookupTable.glTextureExt.numberOfSamples;
                    }
                    this->applyGlTextureExtOverrides(allocation, imgInfo,
                                                     lookupTable.glTextureExt.pGmmResInfo,
                                                     lookupTable.glTextureExt.textureBufferOffset,
                                                     lookupTable.glTextureExt.glHWFormat,
                                                     lookupTable.glTextureExt.isAuxEnabled,
                                                     lookupTable.glTextureExt.numberOfSamples,
                                                     lookupTable.glTextureExt.mcsNtHandle,
                                                     lookupTable.glTextureExt.pGmmResInfoMcs,
                                                     lookupTable.glTextureExt.hasUnifiedMcsSurface);
                }
                if (allocation != nullptr) {
                    auto importedGmm = allocation->getDefaultGmm();
                    importedGmm->queryImageParams(imgInfo);
                }
            }
        } else {

            if (!this->imageFromBuffer) {
                NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, &imgInfo, lookupTable.overrideAllocationType ? lookupTable.imageProperties.imageAllocationType : NEO::AllocationType::image, device->getNEODevice()->getDeviceBitfield());

                properties.flags.preferCompressed = isSuitableForCompression(lookupTable, imgInfo);

                allocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
            } else {

                auto usmAllocation = this->device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(lookupTable.imageProperties.pitchedPtr);
                if (usmAllocation == nullptr) {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }

                auto poolLookup = this->device->getNEODevice()->getDeviceUsmMemAllocPoolFacade().getPoolContainingAlloc(lookupTable.imageProperties.pitchedPtr);
                if (poolLookup.pool && false == poolLookup.isAllocatedInPool()) {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }

                allocation = usmAllocation->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
            }
        }
        if (allocation == nullptr) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
    }

    if (this->imageFromBuffer) {
        imgInfo.linearStorage = true;
        if (desc->type == ZE_IMAGE_TYPE_BUFFER) {
            imgInfo.rowPitch = imgInfo.imgDesc.imageWidth * imgInfo.surfaceFormat->imageElementSizeInBytes;
        } else {
            imgInfo.imgDesc.imageRowPitch = getRowPitchFor2dImage(device, imgInfo);
            if (imgInfo.imgDesc.imageRowPitch > 0) {
                imgInfo.rowPitch = imgInfo.imgDesc.imageRowPitch;
            } else {
                imgInfo.rowPitch = imgInfo.imgDesc.imageWidth * imgInfo.surfaceFormat->imageElementSizeInBytes;
            }
        }
        imgInfo.slicePitch = imgInfo.rowPitch * imgInfo.imgDesc.imageHeight;
        imgInfo.qPitch = 0;
        if (!isImageView()) {
            imgInfo.size = imgInfo.slicePitch;
            imgInfo.offset = ptrDiff(lookupTable.imageProperties.pitchedPtr, allocation->getGpuAddress());
        }
    }

    if (this->device->getGfxCoreHelper().getRenderSurfaceStateSize(rootDeviceEnvironment) == sizeof(RENDER_SURFACE_STATE)) {
        getImplicitArgsSurfaceState() = GfxFamily::cmdInitRenderSurfaceState;
    }

    if (this->bindlessImage) {
        auto result = allocateBindlessSlot();
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        NEO::AllocationProperties imgImplicitArgsAllocProperties(device->getRootDeviceIndex(), NEO::ImageImplicitArgs::getSize(), NEO::AllocationType::buffer, device->getNEODevice()->getDeviceBitfield());
        implicitArgsAllocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(imgImplicitArgsAllocProperties);
    } else if (this->device->getNEODevice()->getBindlessHeapsHelper()) {
        allocateImplicitArgsOnDemand();
    }

    auto gmm = this->allocation->getDefaultGmm();
    auto gmmHelper = static_cast<const NEO::RootDeviceEnvironment &>(rootDeviceEnvironment).getGmmHelper();

    if (!this->imageFromBuffer && gmm != nullptr) {
        NEO::ImagePlane yuvPlaneType = NEO::ImagePlane::noPlane;
        if (isImageView() && isTwoPlaneYuv420Format(sourceImageFormatDesc->format.layout)) {
            yuvPlaneType = NEO::ImagePlane::planeY;
            if (imgInfo.plane == NEO::ImagePlane::planeU) {
                yuvPlaneType = NEO::ImagePlane::planeUV;
            }
        }
        const uint32_t gmmArrayIndex = lookupTable.d3dTextureExt.present
                                           ? lookupTable.d3dTextureExt.arrayIndex
                                           : 0u;
        gmm->updateImgInfoAndDesc(imgInfo, gmmArrayIndex, yuvPlaneType);

        if (yuvPlaneType == NEO::ImagePlane::planeUV) {
            // Callers pass frame dimensions per plane - copy bounds read this descriptor.
            this->imageFormatDesc.width = imgInfo.imgDesc.imageWidth;
            this->imageFormatDesc.height = static_cast<uint32_t>(imgInfo.imgDesc.imageHeight);
        }

        if (lookupTable.isSharedHandle && lookupTable.glTextureExt.present) {
            imgInfo.offset = 0;
            imgInfo.xOffset = 0;
            imgInfo.yOffset = 0;
            imgInfo.yOffsetForUVPlane = 0;
        }

        if (imgInfo.plane == NEO::ImagePlane::noPlane) {
            imgInfo.xOffset = 0;
            imgInfo.yOffset = 0;
        }
    }

    if (lookupTable.imageProperties.customRowPitch > 0) {
        imgInfo.rowPitch = lookupTable.imageProperties.customRowPitch;
        imgInfo.imgDesc.imageRowPitch = lookupTable.imageProperties.customRowPitch;
        this->customRowPitch = lookupTable.imageProperties.customRowPitch;
    } else if (this->customRowPitch > 0) {
        imgInfo.rowPitch = this->customRowPitch;
        imgInfo.imgDesc.imageRowPitch = this->customRowPitch;
    }
    if (lookupTable.imageProperties.customSlicePitch > 0) {
        imgInfo.slicePitch = lookupTable.imageProperties.customSlicePitch;
        imgInfo.imgDesc.imageSlicePitch = lookupTable.imageProperties.customSlicePitch;
        this->customSlicePitch = lookupTable.imageProperties.customSlicePitch;
    } else if (this->customSlicePitch > 0) {
        imgInfo.slicePitch = this->customSlicePitch;
        imgInfo.imgDesc.imageSlicePitch = this->customSlicePitch;
    }

    if (lookupTable.imageTilingOverride.present && lookupTable.imageTilingOverride.rowPitch > 0) {
        imgInfo.rowPitch = lookupTable.imageTilingOverride.rowPitch;
        imgInfo.imgDesc.imageRowPitch = lookupTable.imageTilingOverride.rowPitch;
    }

    // Compute qPitch for 3D images placed on top of a buffer as a function of rows.
    if (this->imageFromBuffer && desc->type == ZE_IMAGE_TYPE_3D) {
        if (imgInfo.rowPitch == 0 || imgInfo.slicePitch % imgInfo.rowPitch != 0) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        const size_t rowsPerSlice = imgInfo.slicePitch / imgInfo.rowPitch;
        // Slice pitch rows must be a multiple of SURFACEQPITCH_ALIGN_SIZE.
        if (rowsPerSlice % RENDER_SURFACE_STATE::SURFACEQPITCH_ALIGN_SIZE != 0) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        imgInfo.qPitch = static_cast<uint32_t>(rowsPerSlice);
        imgInfo.imgDesc.imageSlicePitch = imgInfo.slicePitch;
    }

    imgInfo.print();

    NEO::SurfaceOffsets surfaceOffsets = {imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane};

    const uint32_t cubeFaceIndex = (lookupTable.isSharedHandle && lookupTable.glTextureExt.present)
                                       ? lookupTable.glTextureExt.cubeFaceIndex
                                       : static_cast<uint32_t>(__GMM_NO_CUBE_MAP);

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    const bool usesReducedSurfaceState = gfxCoreHelper.getRenderSurfaceStateSize(rootDeviceEnvironment) < sizeof(RENDER_SURFACE_STATE);

    if (usesReducedSurfaceState && (this->getNumSamples() > 1u)) {
        const bool countTooLarge =
            this->getMcsMultisampleCount() > RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8;
        const bool resolvedThroughControlSurface =
            (this->getMcsAllocation() != nullptr) || this->getIsUnifiedMcsSurface();

        if (countTooLarge || resolvedThroughControlSurface) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    NEO::ImageInfo imgInfoRedescirebed;
    const bool redescribedIsNV12 = (desc->format.layout == ZE_IMAGE_FORMAT_LAYOUT_NV12);
    {
        [[maybe_unused]] uint32_t exponent;
        switch (imgInfo.surfaceFormat->imageElementSizeInBytes) {
        default:
            exponent = Math::log2(imgInfo.surfaceFormat->imageElementSizeInBytes);
            DEBUG_BREAK_IF(exponent >= 5u);
            imgInfoRedescirebed.surfaceFormat = &ImageFormats::surfaceFormatsForRedescribe[exponent % 5];
            break;
        case 3:
            imgInfoRedescirebed.surfaceFormat = &ImageFormats::surfaceFormatsForRedescribe[5];
            break;
        case 6:
            imgInfoRedescirebed.surfaceFormat = &ImageFormats::surfaceFormatsForRedescribe[6];
            break;
        }

        imgInfoRedescirebed.imgDesc = imgInfo.imgDesc;
        imgInfoRedescirebed.qPitch = imgInfo.qPitch;
        if (imgInfoRedescirebed.imgDesc.imageType == NEO::ImageType::image1DBuffer) {
            imgInfoRedescirebed.imgDesc.imageType = NEO::ImageType::image1D;
        }
    }

    const auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();

    SurfaceStateSlotContext slotContext{};
    slotContext.desc = desc;
    slotContext.redescribedImageInfo = &imgInfoRedescirebed;
    slotContext.surfaceOffsets = &surfaceOffsets;
    slotContext.gmm = gmm;
    slotContext.gmmHelper = gmmHelper;
    slotContext.surfaceType = surfaceType;
    slotContext.cubeFaceIndex = cubeFaceIndex;
    slotContext.numSamples = this->getNumSamples();
    slotContext.isMediaFormatLayout = isMediaFormatLayout;
    slotContext.hasFixedChannelSelect = hasFixedChannelSelect;
    slotContext.redescribedIsNV12 = redescribedIsNV12;
    slotContext.packedSupported = productHelper.isPackedCopyFormatSupported();

    encodeSurfaceState(slotContext);

    if (this->bindlessImage) {
        auto ssInHeap = getBindlessSlot();
        copySurfaceStateToSSH(ssInHeap->ssPtr, 0u, NEO::BindlessImageSlot::image, false, 0u);

        if (this->sampledImage) {
            auto productFamily = this->device->getNEODevice()->getHardwareInfo().platform.eProductFamily;
            auto sampler = Sampler::create(productFamily, device, &this->samplerDesc);
            if (!sampler) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }
            auto &gfxCoreHelper = this->device->getGfxCoreHelper();
            auto surfaceStateSize = gfxCoreHelper.getBindlessSurfaceStateSlotSize();
            auto samplerStateOffset = static_cast<uint32_t>(NEO::BindlessImageSlot::sampler * surfaceStateSize);

            ArrayRef<uint8_t> ssInHeapSpan{reinterpret_cast<uint8_t *>(ssInHeap->ssPtr), ssInHeap->ssSize};
            sampler->copySamplerStateToDSH(ssInHeapSpan, samplerStateOffset);
            sampler->destroy();
        }
    }

    if (this->bindlessImage && implicitArgsAllocation) {
        NEO::ImageImplicitArgs imageImplicitArgs{};
        populateImageImplicitArgs(imageImplicitArgs);

        NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *implicitArgsAllocation), *this->device->getNEODevice(), implicitArgsAllocation, 0u, &imageImplicitArgs, NEO::ImageImplicitArgs::getSize());
        this->encodeImplicitArgsSurfaceState();
        auto surfaceStateSize = this->device->getGfxCoreHelper().getBindlessSurfaceStateSlotSize();
        auto ssInHeap = getBindlessSlot();
        copySurfaceStateToSSH(ptrOffset(ssInHeap->ssPtr, surfaceStateSize), 0u, NEO::BindlessImageSlot::implicitArgs, false, 0u);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::encodeSurfaceState(const SurfaceStateSlotContext &context) {
    const auto &rootDeviceEnvironment = this->device->getNEODevice()->getRootDeviceEnvironment();
    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    if (gfxCoreHelper.getRenderSurfaceStateSize(rootDeviceEnvironment) < sizeof(RENDER_SURFACE_STATE)) {
        encodeSurfaceStateReduced(context);
    } else {
        encodeSurfaceStateFull(context);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::encodeSurfaceStateReduced(const SurfaceStateSlotContext &context) {
    auto &gfxCoreHelper = this->device->getGfxCoreHelper();

    const auto *desc = context.desc;
    auto *gmm = context.gmm;
    auto *gmmHelper = context.gmmHelper;
    const auto &surfaceOffsets = *context.surfaceOffsets;
    const auto &imgInfoRedescirebed = *context.redescribedImageInfo;
    const auto cubeFaceIndex = context.cubeFaceIndex;
    const auto numSamplesForSurfaceState = context.numSamples;
    const bool isMediaFormatLayout = context.isMediaFormatLayout;
    const bool hasFixedChannelSelect = context.hasFixedChannelSelect;
    const bool redescribedIsNV12 = context.redescribedIsNV12;

    auto makeInputsFromSource = [&](const NEO::ImageInfo &encodeImageInfo, bool isNV12) {
        using NUMBER_OF_MULTISAMPLES = typename RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES;

        NEO::ImageSurfaceStateInputs inputs{};
        inputs.imageInfo = &encodeImageInfo;
        inputs.gmm = gmm;
        inputs.gmmHelper = gmmHelper;
        inputs.surfaceOffsets = &surfaceOffsets;
        inputs.gpuAddress = this->allocation->getGpuAddress();
        inputs.cubeFaceIndex = cubeFaceIndex;
        inputs.useChannelSelects = true;
        inputs.isNV12Format = isNV12;

        inputs.numberOfMultisamples =
            (numSamplesForSurfaceState <= 1)
                ? static_cast<uint32_t>(NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1)
                : static_cast<uint32_t>(this->getMcsMultisampleCount());

        inputs.multisampleControlSurfacePresent =
            (inputs.numberOfMultisamples > 0u) &&
            ((this->getMcsAllocation() != nullptr) || this->getIsUnifiedMcsSurface());

        return inputs;
    };

    auto makeImageSlotInputs = [&]() {
        auto inputs = makeInputsFromSource(imgInfo, isMediaFormatLayout);

        if (!hasFixedChannelSelect) {
            inputs.shaderChannelSelectRed = static_cast<uint32_t>(shaderChannelSelect[desc->format.x]);
            inputs.shaderChannelSelectGreen = static_cast<uint32_t>(shaderChannelSelect[desc->format.y]);
            inputs.shaderChannelSelectBlue = static_cast<uint32_t>(shaderChannelSelect[desc->format.z]);
            inputs.shaderChannelSelectAlpha = static_cast<uint32_t>(shaderChannelSelect[desc->format.w]);
        } else {
            inputs.shaderChannelSelectRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
            inputs.shaderChannelSelectGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
            inputs.shaderChannelSelectBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
            inputs.shaderChannelSelectAlpha = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE;
        }

        inputs.isDepthStencilResource =
            (gmm != nullptr) && (this->depthStencilImage || gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth);
        return inputs;
    };

    auto makeRedescribedSlotInputs = [&]() {
        auto inputs = makeInputsFromSource(imgInfoRedescirebed, redescribedIsNV12);

        const auto redescribedGmmFormat = imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat;
        inputs.shaderChannelSelectRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
        if (redescribedGmmFormat == GMM_FORMAT_R8_UINT_TYPE ||
            redescribedGmmFormat == GMM_FORMAT_R16_UINT_TYPE ||
            redescribedGmmFormat == GMM_FORMAT_R32_UINT_TYPE) {
            inputs.shaderChannelSelectGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            inputs.shaderChannelSelectBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        } else if (redescribedGmmFormat == GMM_FORMAT_R32G32_UINT_TYPE) {
            inputs.shaderChannelSelectGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
            inputs.shaderChannelSelectBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        } else {
            inputs.shaderChannelSelectGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
            inputs.shaderChannelSelectBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;
        }
        inputs.shaderChannelSelectAlpha = redescribedIsNV12
                                              ? RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE
                                              : RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA;

        inputs.isDepthStencilResource = (gmm != nullptr) && gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth;
        return inputs;
    };

    auto makePackedSlotInputs = [&]() {
        auto inputs = makeRedescribedSlotInputs();
        inputs.imageInfo = &imgInfo;
        inputs.isNV12Format = isMediaFormatLayout;

        RENDER_SURFACE_STATE packedView = GfxFamily::cmdInitRenderSurfaceState;
        NEO::EncodeSurfaceState<GfxFamily>::convertSurfaceStateToPacked(&packedView, imgInfo);

        inputs.shaderChannelSelectRed = static_cast<uint32_t>(packedView.getShaderChannelSelectRed());
        inputs.shaderChannelSelectGreen = static_cast<uint32_t>(packedView.getShaderChannelSelectGreen());
        inputs.shaderChannelSelectBlue = static_cast<uint32_t>(packedView.getShaderChannelSelectBlue());
        inputs.shaderChannelSelectAlpha = static_cast<uint32_t>(packedView.getShaderChannelSelectAlpha());

        if (static_cast<uint32_t>(packedView.getSurfaceFormat()) !=
            static_cast<uint32_t>(imgInfo.surfaceFormat->genxSurfaceFormat)) {
            inputs.packedFormatOverride = true;
            inputs.packedSurfaceFormat = static_cast<uint32_t>(packedView.getSurfaceFormat());
            inputs.packedWidth = packedView.getWidth();
            inputs.packedTileBpp = NEO::getSurfaceStateOverrideTileBppIfSupported(packedView);
        }
        return inputs;
    };

    auto imageInputs = makeImageSlotInputs();
    gfxCoreHelper.encodeImageSurfaceState(surfaceStateStorage.data(), imageInputs);

    auto redescribedInputs = makeRedescribedSlotInputs();
    gfxCoreHelper.encodeImageSurfaceState(redescribedSurfaceStateStorage.data(), redescribedInputs);

    if (context.packedSupported) {
        auto packedInputs = makePackedSlotInputs();
        gfxCoreHelper.encodeImageSurfaceState(packedSurfaceStateStorage.data(), packedInputs);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::encodeSurfaceStateFull(const SurfaceStateSlotContext &context) {

    const auto *desc = context.desc;
    auto *gmm = context.gmm;
    auto *gmmHelper = context.gmmHelper;
    const auto &surfaceOffsets = *context.surfaceOffsets;
    const auto &imgInfoRedescirebed = *context.redescribedImageInfo;
    const auto cubeFaceIndex = context.cubeFaceIndex;
    const auto numSamplesForSurfaceState = context.numSamples;
    const bool isMediaFormatLayout = context.isMediaFormatLayout;
    const bool hasFixedChannelSelect = context.hasFixedChannelSelect;
    const bool redescribedIsNV12 = context.redescribedIsNV12;
    const auto surfaceType = context.surfaceType;

    auto programMultisampleSurfaceState = [&](RENDER_SURFACE_STATE *surfaceState) {
        using NUMBER_OF_MULTISAMPLES = typename RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES;

        if (numSamplesForSurfaceState <= 1) {
            surfaceState->setNumberOfMultisamples(NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);
            return;
        }
        surfaceState->setNumberOfMultisamples(static_cast<NUMBER_OF_MULTISAMPLES>(this->getMcsMultisampleCount()));

        auto mcsAlloc = this->getMcsAllocation();
        const bool unifiedMcs = this->getIsUnifiedMcsSurface();
        if (mcsAlloc != nullptr || unifiedMcs) {
            auto mcsGmm = mcsAlloc ? mcsAlloc->getDefaultGmm() : gmm;
            const auto &hwInfo = device->getNEODevice()->getHardwareInfo();
            NEO::EncodeSurfaceState<GfxFamily>::setAuxParamsForMCSCCS(surfaceState, hwInfo);
            surfaceState->setAuxiliarySurfacePitch(mcsGmm->getUnifiedAuxPitchTiles());
            surfaceState->setAuxiliarySurfaceQPitch(mcsGmm->getAuxQPitch());
            NEO::EncodeSurfaceState<GfxFamily>::setClearColorParams(surfaceState, mcsGmm);
            NEO::ImageSurfaceStateHelper<GfxFamily>::setUnifiedAuxBaseAddress(surfaceState, mcsGmm);
        } else {
            using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
            const bool isDepthResource = this->depthStencilImage || (gmm && gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth);
            if (isDepthResource && surfaceState->getSurfaceFormat() != SURFACE_FORMAT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS) {
                surfaceState->setMultisampledSurfaceStorageFormat(
                    RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
            } else {
                surfaceState->setMultisampledSurfaceStorageFormat(
                    RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
            }
        }
    };

    auto buildFullImageSurfaceState = [&](RENDER_SURFACE_STATE &dst) {
        uint32_t minArrayElement, renderTargetViewExtent, depth;
        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceState(&dst, imgInfo, gmm, *gmmHelper, cubeFaceIndex,
                                                                      this->allocation->getGpuAddress(), surfaceOffsets,
                                                                      isMediaFormatLayout, minArrayElement, renderTargetViewExtent);

        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceStateDimensions(&dst, imgInfo, cubeFaceIndex, surfaceType, depth);
        dst.setSurfaceMinLOD(0u);
        dst.setMIPCountLOD(0u);
        NEO::ImageSurfaceStateHelper<GfxFamily>::setMipTailStartLOD(&dst, gmm);

        if (!hasFixedChannelSelect) {
            dst.setShaderChannelSelectRed(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.x]));
            dst.setShaderChannelSelectGreen(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.y]));
            dst.setShaderChannelSelectBlue(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.z]));
            dst.setShaderChannelSelectAlpha(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.w]));
        } else {
            dst.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            dst.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
            dst.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
            dst.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
        }

        programMultisampleSurfaceState(&dst);

        if (numSamplesForSurfaceState <= 1 && allocation->isCompressionEnabled()) {
            NEO::EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(&dst, gmm);
        }

        if (gmm) {
            const bool isDepthResource = this->depthStencilImage || gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth;
            dst.setDepthStencilResource(isDepthResource);
        }
    };

    auto buildFullRedescribedSurfaceState = [&](RENDER_SURFACE_STATE &dst) {
        uint32_t minArrayElement, renderTargetViewExtent, depth;
        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceState(&dst, imgInfoRedescirebed, gmm, *gmmHelper,
                                                                      cubeFaceIndex, this->allocation->getGpuAddress(), surfaceOffsets,
                                                                      redescribedIsNV12, minArrayElement, renderTargetViewExtent);

        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceStateDimensions(&dst, imgInfoRedescirebed, cubeFaceIndex, surfaceType, depth);
        dst.setSurfaceMinLOD(0u);
        dst.setMIPCountLOD(0u);
        NEO::ImageSurfaceStateHelper<GfxFamily>::setMipTailStartLOD(&dst, gmm);

        if (imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R8_UINT_TYPE ||
            imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R16_UINT_TYPE ||
            imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R32_UINT_TYPE) {
            dst.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            dst.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
            dst.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
        } else if (imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R32G32_UINT_TYPE) {
            dst.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            dst.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
            dst.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
        } else {
            dst.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            dst.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
            dst.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
        }

        programMultisampleSurfaceState(&dst);

        if (numSamplesForSurfaceState <= 1 && allocation->isCompressionEnabled()) {
            NEO::EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(&dst, gmm);
        }

        if (gmm) {
            const bool isDepthResource = gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth;
            dst.setDepthStencilResource(isDepthResource);
        }
    };

    auto &surfaceState = this->getSurfaceState();
    auto &redescribedSurfaceState = this->getRedescribedSurfaceState();
    auto &packedSurfaceState = this->getPackedSurfaceState();

    surfaceState = GfxFamily::cmdInitRenderSurfaceState;
    packedSurfaceState = GfxFamily::cmdInitRenderSurfaceState;
    buildFullImageSurfaceState(surfaceState);

    redescribedSurfaceState = GfxFamily::cmdInitRenderSurfaceState;
    buildFullRedescribedSurfaceState(redescribedSurfaceState);

    if (context.packedSupported) {
        packedSurfaceState = redescribedSurfaceState;
        NEO::EncodeSurfaceState<GfxFamily>::convertSurfaceStateToPacked(&packedSurfaceState, imgInfo);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::copySurfaceStateToSSH(void *surfaceStateHeap,
                                                           uint32_t surfaceStateOffset,
                                                           uint32_t bindlessSlot,
                                                           bool isMediaBlockArg,
                                                           uint32_t mipLevel) {
    const void *src = nullptr;

    switch (bindlessSlot) {
    case NEO::BindlessImageSlot::image:
        src = surfaceStateStorage.data();
        break;
    case NEO::BindlessImageSlot::redescribedImage:
        src = redescribedSurfaceStateStorage.data();
        break;
    case NEO::BindlessImageSlot::implicitArgs:
        src = implicitArgsSurfaceStateStorage.data();
        break;
    case NEO::BindlessImageSlot::packedImage:
        src = packedSurfaceStateStorage.data();
        break;
    default:
        UNRECOVERABLE_IF(true);
    }

    auto dst = ptrOffset(surfaceStateHeap, surfaceStateOffset);
    const auto &rootDeviceEnvironment = this->device->getNEODevice()->getRootDeviceEnvironment();
    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    const size_t stateSize = gfxCoreHelper.getRenderSurfaceStateSize(rootDeviceEnvironment);

    memcpy_s(dst, stateSize, src, stateSize);

    if (bindlessSlot == NEO::BindlessImageSlot::implicitArgs) {
        return;
    }

    auto *gmm = (this->allocation != nullptr) ? this->allocation->getDefaultGmm() : nullptr;
    gfxCoreHelper.applyImageSurfaceStateMipAndMediaBlock(dst, imgInfo, gmm, mipLevel, isMediaBlockArg, rootDeviceEnvironment);
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool ImageCoreFamily<gfxCoreFamily>::isSuitableForCompression(const StructuresLookupTable &structuresLookupTable, const NEO::ImageInfo &imgInfo) {
    auto &hwInfo = device->getHwInfo();
    auto &loGfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    if (structuresLookupTable.uncompressedHint) {
        return false;
    }

    return (loGfxCoreHelper.imageCompressionSupported(hwInfo) && !imgInfo.linearStorage);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::encodeImplicitArgsSurfaceState() {

    auto &helper = this->device->getGfxCoreHelper();
    auto gmmHelper = device->getNEODevice()->getGmmHelper();

    NEO::EncodeSurfaceStateArgs encodeArgs;
    encodeArgs.outMemory = implicitArgsSurfaceStateStorage.data();
    encodeArgs.size = NEO::ImageImplicitArgs::getSize();
    encodeArgs.graphicsAddress = implicitArgsAllocation->getGpuAddress();
    encodeArgs.gmmHelper = gmmHelper;
    encodeArgs.allocation = implicitArgsAllocation;
    encodeArgs.numAvailableDevices = this->device->getNEODevice()->getNumGenericSubDevices();
    encodeArgs.areMultipleSubDevicesInContext = encodeArgs.numAvailableDevices > 1;
    encodeArgs.mocs = helper.getMocsIndex(*encodeArgs.gmmHelper, true, false) << 1;
    encodeArgs.implicitScaling = this->device->isImplicitScalingCapable();
    encodeArgs.isDebuggerActive = this->device->getNEODevice()->getDebugger() != nullptr;

    helper.encodeBufferSurfaceState(encodeArgs);
}
} // namespace L0
