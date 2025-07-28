/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {

class GfxCoreHelper;
class Gmm;
class Image;
class ReleaseHelper;
struct HardwareInfo;
struct KernelInfo;
struct SurfaceFormatInfo;
struct UnifiedSharingMemoryDescription;
class MemoryManager;
class SharingHandler;

using ImageCreateFunc = Image *(*)(Context *context,
                                   const MemoryProperties &memoryProperties,
                                   uint64_t flags,
                                   uint64_t flagsIntel,
                                   size_t size,
                                   void *hostPtr,
                                   const cl_image_format &imageFormat,
                                   const cl_image_desc &imageDesc,
                                   bool zeroCopy,
                                   MultiGraphicsAllocation multiGraphicsAllocation,
                                   bool isImageRedescribed,
                                   uint32_t baseMipLevel,
                                   uint32_t mipCount,
                                   const ClSurfaceFormatInfo *surfaceFormatInfo,
                                   const SurfaceOffsets *surfaceOffsets);

struct ImageFactoryFuncs {
    ImageCreateFunc createImageFunction;
};

namespace ImageFunctions {
using ValidateAndCreateImageFunc = std::function<cl_mem(cl_context context,
                                                        const uint64_t *properties,
                                                        uint64_t flags,
                                                        uint64_t flagsIntel,
                                                        const cl_image_format *imageFormat,
                                                        const cl_image_desc *imageDesc,
                                                        const void *hostPtr,
                                                        int32_t &errcodeRet)>;
extern ValidateAndCreateImageFunc validateAndCreateImage;
} // namespace ImageFunctions

class Image : public MemObj {
  public:
    const static cl_ulong maskMagic = 0xFFFFFFFFFFFFFFFFLL;
    static const cl_ulong objectMagic = MemObj::objectMagic | 0x01;

    ~Image() override;

    static Image *create(Context *context,
                         const MemoryProperties &memoryProperties,
                         cl_mem_flags flags,
                         cl_mem_flags_intel flagsIntel,
                         const ClSurfaceFormatInfo *surfaceFormat,
                         const cl_image_desc *imageDesc,
                         const void *hostPtr,
                         cl_int &errcodeRet);

    static cl_mem validateAndCreateImage(cl_context context,
                                         const cl_mem_properties *properties,
                                         cl_mem_flags flags,
                                         cl_mem_flags_intel flagsIntel,
                                         const cl_image_format *imageFormat,
                                         const cl_image_desc *imageDesc,
                                         const void *hostPtr,
                                         cl_int &errcodeRet);

    static Image *createImageHw(Context *context, const MemoryProperties &memoryProperties, cl_mem_flags flags,
                                cl_mem_flags_intel flagsIntel, size_t size, void *hostPtr,
                                const cl_image_format &imageFormat, const cl_image_desc &imageDesc,
                                bool zeroCopy, MultiGraphicsAllocation multiGraphicsAllocation,
                                bool isObjectRedescribed, uint32_t baseMipLevel, uint32_t mipCount, const ClSurfaceFormatInfo *surfaceFormatInfo = nullptr);

    static Image *createSharedImage(Context *context, SharingHandler *sharingHandler, const McsSurfaceInfo &mcsSurfaceInfo,
                                    MultiGraphicsAllocation multiGraphicsAllocation, GraphicsAllocation *mcsAllocation,
                                    cl_mem_flags flags, cl_mem_flags_intel flagsIntel, const ClSurfaceFormatInfo *surfaceFormat, ImageInfo &imgInfo, uint32_t cubeFaceIndex, uint32_t baseMipLevel, uint32_t mipCount, bool hasUnifiedMcsSurface);

    static cl_int validate(Context *context,
                           const MemoryProperties &memoryProperties,
                           const ClSurfaceFormatInfo *surfaceFormat,
                           const cl_image_desc *imageDesc,
                           const void *hostPtr);
    static cl_int validateImageFormat(const cl_image_format *imageFormat);

    static int32_t validatePlanarYUV(Context *context,
                                     const MemoryProperties &memoryProperties,
                                     const cl_image_desc *imageDesc,
                                     const void *hostPtr);

    static int32_t validatePackedYUV(const MemoryProperties &memoryProperties, const cl_image_desc *imageDesc);

    static cl_int validateImageTraits(Context *context, const MemoryProperties &memoryProperties, const cl_image_format *imageFormat, const cl_image_desc *imageDesc, const void *hostPtr);

    static size_t calculateHostPtrSize(const size_t *region, size_t rowPitch, size_t slicePitch, size_t pixelSize, uint32_t imageType);

    static void calculateHostPtrOffset(size_t *imageOffset, const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch, uint32_t imageType, size_t bytesPerPixel);

    static cl_int getImageParams(Context *context,
                                 cl_mem_flags flags,
                                 const ClSurfaceFormatInfo *surfaceFormat,
                                 const cl_image_desc *imageDesc,
                                 size_t *imageRowPitch,
                                 size_t *imageSlicePitch);

    static bool isImage1d(const cl_image_desc &imageDesc);
    static bool isImage2d(cl_mem_object_type imageType);
    static bool isImage3d(cl_mem_object_type imageType);
    static bool isImage2dOr2dArray(cl_mem_object_type imageType);
    static bool isImageArray(cl_mem_object_type imageType);

    static bool isDepthFormat(const cl_image_format &imageFormat);

    static bool hasSlices(cl_mem_object_type type) {
        return (type == CL_MEM_OBJECT_IMAGE3D) || (type == CL_MEM_OBJECT_IMAGE1D_ARRAY) || (type == CL_MEM_OBJECT_IMAGE2D_ARRAY);
    }

    static ImageType convertType(const cl_mem_object_type type);
    static cl_mem_object_type convertType(const ImageType type);
    static ImageDescriptor convertDescriptor(const cl_image_desc &imageDesc);
    static cl_image_desc convertDescriptor(const ImageDescriptor &imageDesc);

    cl_int getImageInfo(cl_image_info paramName,
                        size_t paramValueSize,
                        void *paramValue,
                        size_t *paramValueSizeRet);

    virtual void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel, uint32_t rootDeviceIndex) = 0;

    const cl_image_desc &getImageDesc() const;
    const cl_image_format &getImageFormat() const;
    const ClSurfaceFormatInfo &getSurfaceFormatInfo() const;

    void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;
    void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;

    Image *redescribe();
    Image *redescribeFillImage();
    ImageCreateFunc createFunction = nullptr;

    uint32_t getQPitch() { return qPitch; }
    void setQPitch(uint32_t qPitch) { this->qPitch = qPitch; }
    size_t getHostPtrRowPitch() const { return hostPtrRowPitch; }
    void setHostPtrRowPitch(size_t pitch) { this->hostPtrRowPitch = pitch; }
    size_t getHostPtrSlicePitch() const { return hostPtrSlicePitch; }
    void setHostPtrSlicePitch(size_t pitch) { this->hostPtrSlicePitch = pitch; }
    size_t getImageCount() const { return imageCount; }
    void setImageCount(size_t imageCount) { this->imageCount = imageCount; }
    void setImageRowPitch(size_t rowPitch) { imageDesc.image_row_pitch = rowPitch; }
    void setImageSlicePitch(size_t slicePitch) { imageDesc.image_slice_pitch = slicePitch; }
    void setSurfaceOffsets(uint64_t offset, uint32_t xOffset, uint32_t yOffset, uint32_t yOffsetForUVPlane) {
        surfaceOffsets.offset = offset;
        surfaceOffsets.xOffset = xOffset;
        surfaceOffsets.yOffset = yOffset;
        surfaceOffsets.yOffsetForUVplane = yOffsetForUVPlane;
    }
    void getSurfaceOffsets(SurfaceOffsets &surfaceOffsetsOut) { surfaceOffsetsOut = this->surfaceOffsets; }
    bool getIsDisplayable() const { return isDisplayable; }
    void setIsDisplayable(bool displayable) { this->isDisplayable = displayable; }

    void setCubeFaceIndex(uint32_t index) { cubeFaceIndex = index; }
    uint32_t getCubeFaceIndex() { return cubeFaceIndex; }
    void setMediaPlaneType(cl_uint type) { mediaPlaneType = type; }
    cl_uint getMediaPlaneType() const { return mediaPlaneType; }
    int peekBaseMipLevel() { return baseMipLevel; }
    void setBaseMipLevel(int level) { this->baseMipLevel = level; }

    uint32_t peekMipCount() { return mipCount; }
    void setMipCount(uint32_t mipCountNew) { this->mipCount = mipCountNew; }

    static const ClSurfaceFormatInfo *getSurfaceFormatFromTable(cl_mem_flags flags, const cl_image_format *imageFormat, bool supportsOcl20Features);
    static cl_int validateRegionAndOrigin(const size_t *origin, const size_t *region, const cl_image_desc &imgDesc);

    cl_int writeNV12Planes(const void *hostPtr, size_t hostPtrRowPitch, uint32_t rootDeviceIndex);
    void setMcsSurfaceInfo(const McsSurfaceInfo &info) { mcsSurfaceInfo = info; }
    const McsSurfaceInfo &getMcsSurfaceInfo() { return mcsSurfaceInfo; }
    void setPlane(const GMM_YUV_PLANE_ENUM plane) { this->plane = plane; }
    GMM_YUV_PLANE_ENUM getPlane() const { return this->plane; }
    size_t calculateOffsetForMapping(const MemObjOffsetArray &origin) const override;

    virtual void transformImage2dArrayTo3d(void *memory) = 0;
    virtual void transformImage3dTo2dArray(void *memory) = 0;

    bool hasSameDescriptor(const cl_image_desc &imageDesc) const;
    bool hasValidParentImageFormat(const cl_image_format &imageFormat) const;

    bool isImageFromBuffer() const { return castToObject<Buffer>(static_cast<cl_mem>(associatedMemObject)) ? true : false; }
    bool isImageFromImage() const { return castToObject<Image>(static_cast<cl_mem>(associatedMemObject)) ? true : false; }

    static cl_int checkIfDeviceSupportsImages(cl_context context);

    void fillImageRegion(size_t *region) const;

    static bool validateHandleType(MemoryProperties &memoryProperties, UnifiedSharingMemoryDescription &extMem);
    void setAs3DUavOrRtvImage(bool isUavOrRtv);
    void setIsPackedFormat(bool isPackedFormat) { this->isPackedFormat = isPackedFormat; }

  protected:
    Image(Context *context,
          const MemoryProperties &memoryProperties,
          cl_mem_flags flags,
          cl_mem_flags_intel flagsIntel,
          size_t size,
          void *memoryStorage,
          void *hostPtr,
          cl_image_format imageFormat,
          const cl_image_desc &imageDesc,
          bool zeroCopy,
          MultiGraphicsAllocation multiGraphicsAllocation,
          bool isObjectRedescribed,
          uint32_t baseMipLevel,
          uint32_t mipCount,
          const ClSurfaceFormatInfo &surfaceFormatInfo,
          const SurfaceOffsets *surfaceOffsets = nullptr);

    void getOsSpecificImageInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam);

    MOCKABLE_VIRTUAL void transferData(void *dst, size_t dstRowPitch, size_t dstSlicePitch,
                                       void *src, size_t srcRowPitch, size_t srcSlicePitch,
                                       std::array<size_t, 3> copyRegion, std::array<size_t, 3> copyOrigin);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    ClSurfaceFormatInfo surfaceFormatInfo;
    McsSurfaceInfo mcsSurfaceInfo = {};
    uint32_t qPitch = 0;
    size_t hostPtrRowPitch = 0;
    size_t hostPtrSlicePitch = 0;
    size_t imageCount = 0;
    uint32_t cubeFaceIndex = __GMM_NO_CUBE_MAP;
    cl_uint mediaPlaneType = 0;
    SurfaceOffsets surfaceOffsets = {0};
    uint32_t baseMipLevel = 0;
    uint32_t mipCount = 1;
    GMM_YUV_PLANE_ENUM plane = GMM_NO_PLANE;
    bool is3DUAVOrRTV = false;
    bool isPackedFormat = false;

    static bool isValidSingleChannelFormat(const cl_image_format *imageFormat);
    static bool isValidIntensityFormat(const cl_image_format *imageFormat);
    static bool isValidLuminanceFormat(const cl_image_format *imageFormat);
    static bool isValidDepthFormat(const cl_image_format *imageFormat);
    static bool isValidDoubleChannelFormat(const cl_image_format *imageFormat);
    static bool isValidTripleChannelFormat(const cl_image_format *imageFormat);
    static bool isValidRGBAFormat(const cl_image_format *imageFormat);
    static bool isValidSRGBFormat(const cl_image_format *imageFormat);
    static bool isValidARGBFormat(const cl_image_format *imageFormat);
    static bool isValidDepthStencilFormat(const cl_image_format *imageFormat);
    static bool isValidYUVFormat(const cl_image_format *imageFormat);
    static bool hasAlphaChannel(const cl_image_format *imageFormat);
    static size_t getImageHeight(const cl_image_desc &imageDesc);
    static size_t getImageDepth(const cl_image_desc &imageDesc);
    static size_t getHostPtrMinSize(cl_mem_object_type imageType, const cl_image_format &imageFormat,
                                    size_t hostPtrRowPitch, size_t hostPtrSlicePitch, size_t imageHeight, size_t imageDepth, size_t imageCount);
    static size_t getHostPtrSlicePitch(const cl_image_desc &imageDesc, size_t hostPtrRowPitch, size_t imageHeight);
    static bool isParentMemObject(const cl_image_desc &imageDesc);
    static bool isImageFromBuffer(const cl_image_desc &imageDesc, Buffer *buffer);
    static void setImageProperties(Image *image, const cl_image_desc &imageDesc, const ImageInfo &imageInfo, Image *parentImage, Buffer *parentBuffer,
                                   size_t hostPtrRowPitch, size_t hostPtrSlicePitch, size_t imageCount, size_t hostPtrMinSize);

    static void adjustImagePropertiesFromParentImage(size_t &width, size_t &height, size_t &depth, ImageInfo &imageInfo, cl_image_desc &descriptor, Image *parentImage);

    static void setAllocationInfoFromParentBuffer(CreateMemObj::AllocationInfo &allocationInfo, const void *&hostPtr, void *&hostPtrToSet,
                                                  Buffer *parentBuffer, ImageInfo &imageInfo, uint32_t rootDeviceIndex);

    static void setAllocationInfoFromHostPtr(CreateMemObj::AllocationInfo &allocationInfo, uint32_t rootDeviceIndex, const HardwareInfo &hwInfo,
                                             const MemoryProperties &memoryProperties, ImageInfo &imageInfo, Context *context, bool preferCompression,
                                             MemoryManager *memoryManager, const void *hostPtr, size_t hostPtrMinSize);

    static void setAllocationInfoFromImageInfo(CreateMemObj::AllocationInfo &allocationInfo, uint32_t rootDeviceIndex, const HardwareInfo &hwInfo,
                                               const MemoryProperties &memoryProperties, ImageInfo &imageInfo, Context *context, bool preferCompression,
                                               MemoryManager *memoryManager);

    static void providePerformanceHintForCreateImage(Image *image, const HardwareInfo &hwInfo, CreateMemObj::AllocationInfo &allocationInfo, Context *context);

    static void setImageDesriptorIfParentImage(cl_image_desc &imageDescriptor, size_t imageWidth, size_t imageHeight, cl_mem memObject);
};

template <typename GfxFamily>
class ImageHw : public Image {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

  public:
    ImageHw(Context *context,
            const MemoryProperties &memoryProperties,
            cl_mem_flags flags,
            cl_mem_flags_intel flagsIntel,
            size_t size,
            void *memoryStorage,
            void *hostPtr,
            const cl_image_format &imageFormat,
            const cl_image_desc &imageDesc,
            bool zeroCopy,
            MultiGraphicsAllocation multiGraphicsAllocation,
            bool isObjectRedescribed,
            uint32_t baseMipLevel,
            uint32_t mipCount,
            const ClSurfaceFormatInfo &surfaceFormatInfo,
            const SurfaceOffsets *surfaceOffsets = nullptr)
        : Image(context, memoryProperties, flags, flagsIntel, size, memoryStorage, hostPtr, imageFormat, imageDesc,
                zeroCopy, std::move(multiGraphicsAllocation), isObjectRedescribed, baseMipLevel, mipCount, surfaceFormatInfo, surfaceOffsets) {

        if (!isImage3d(imageDesc.image_type)) {
            this->imageDesc.image_depth = 0;
        }

        switch (imageDesc.image_type) {
        case CL_MEM_OBJECT_IMAGE1D:
        case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
            surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
            break;
        default:
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        case CL_MEM_OBJECT_IMAGE2D:
            surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D;
            break;
        case CL_MEM_OBJECT_IMAGE3D:
            surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D;
            break;
        }
    }

    void setImageArg(void *memory, bool setAsMediaBlockImage, uint32_t mipLevel, uint32_t rootDeviceIndex) override;
    void setAuxParamsForMultisamples(RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex);
    void appendSurfaceStateParams(RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex);
    void appendSurfaceStateDepthParams(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm);
    void appendSurfaceStateExt(void *memory);
    void transformImage2dArrayTo3d(void *memory) override;
    void transformImage3dTo2dArray(void *memory) override;
    static void adjustDepthLimitations(RENDER_SURFACE_STATE *surfaceState, uint32_t minArrayElement, uint32_t renderTargetViewExtent, uint32_t depth, uint32_t mipCount, bool is3DUavOrRtv);
    static Image *create(Context *context,
                         const MemoryProperties &memoryProperties,
                         cl_mem_flags flags,
                         cl_mem_flags_intel flagsIntel,
                         size_t size,
                         void *hostPtr,
                         const cl_image_format &imageFormat,
                         const cl_image_desc &imageDesc,
                         bool zeroCopy,
                         MultiGraphicsAllocation multiGraphicsAllocation,
                         bool isObjectRedescribed,
                         uint32_t baseMipLevel,
                         uint32_t mipCount,
                         const ClSurfaceFormatInfo *surfaceFormatInfo,
                         const SurfaceOffsets *surfaceOffsets) {
        UNRECOVERABLE_IF(surfaceFormatInfo == nullptr);
        auto memoryStorage = multiGraphicsAllocation.getDefaultGraphicsAllocation()->getUnderlyingBuffer();
        return new ImageHw<GfxFamily>(context,
                                      memoryProperties,
                                      flags,
                                      flagsIntel,
                                      size,
                                      memoryStorage,
                                      hostPtr,
                                      imageFormat,
                                      imageDesc,
                                      zeroCopy,
                                      std::move(multiGraphicsAllocation),
                                      isObjectRedescribed,
                                      baseMipLevel,
                                      mipCount,
                                      *surfaceFormatInfo,
                                      surfaceOffsets);
    }

    static int getShaderChannelValue(int inputShaderChannel, cl_channel_order imageChannelOrder) {
        if (imageChannelOrder == CL_A) {
            if (inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED ||
                inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN ||
                inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE) {
                return RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            }
        } else if (imageChannelOrder == CL_R ||
                   imageChannelOrder == CL_RA ||
                   imageChannelOrder == CL_Rx ||
                   imageChannelOrder == CL_DEPTH) {
            if (inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN ||
                inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE) {
                return RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            }
        } else if (imageChannelOrder == CL_RG ||
                   imageChannelOrder == CL_RGx) {
            if (inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE) {
                return RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            }
        }
        return inputShaderChannel;
    }
    typename RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType;
};

} // namespace NEO
