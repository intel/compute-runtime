/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

template <typename Family>
void EncodeSurfaceState<Family>::appendImageCompressionParams(R_SURFACE_STATE *surfaceState, GraphicsAllocation *allocation, GmmHelper *gmmHelper, bool imageFromBuffer,
                                                              ImagePlane plane) {
    const auto ccsMode = R_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E;
    const auto mcsLceMode = R_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE;
    if ((ccsMode == surfaceState->getAuxiliarySurfaceMode() || mcsLceMode == surfaceState->getAuxiliarySurfaceMode() || surfaceState->getMemoryCompressionEnable())) {
        uint32_t compressionFormat;
        auto gmmResourceInfo = allocation->getDefaultGmm()->gmmResourceInfo.get();
        if (gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
            compressionFormat = gmmHelper->getClientContext()->getMediaSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
            EncodeWA<Family>::adjustCompressionFormatForPlanarImage(compressionFormat, static_cast<int>(plane));
        } else {
            compressionFormat = gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
        }

        if (imageFromBuffer) {
            if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
                compressionFormat = debugManager.flags.ForceBufferCompressionFormat.get();
            }
            appendParamsForImageFromBuffer(surfaceState);
        }

        surfaceState->setCompressionFormat(compressionFormat);
    }
}

template <typename Family>
void EncodeSurfaceState<Family>::setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType) {
    surfaceState->setCoherencyType(R_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
}

template <typename Family>
void EncodeWA<Family>::adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, int plane) {
    static_assert(sizeof(plane) == sizeof(GMM_YUV_PLANE_ENUM));
    if (plane == GMM_PLANE_Y) {
        compressionFormat &= 0xf;
    } else if ((plane == GMM_PLANE_U) || (plane == GMM_PLANE_V)) {
        compressionFormat |= 0x10;
    }
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setupPreferredSlmSize(InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename InterfaceDescriptorType::PREFERRED_SLM_ALLOCATION_SIZE;
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const uint32_t threadsPerDssCount = EncodeDispatchKernel<Family>::getThreadCountPerSubslice(hwInfo);
    const uint32_t workGroupCountPerDss = static_cast<uint32_t>(Math::divideAndRoundUp(threadsPerDssCount, threadsPerThreadGroup));

    slmTotalSize = EncodeDispatchKernel<Family>::alignPreferredSlmSize(slmTotalSize);

    uint32_t slmSize = 0u;

    switch (slmPolicy) {
    case SlmPolicy::slmPolicyLargeData:
        slmSize = slmTotalSize;
        break;
    case SlmPolicy::slmPolicyLargeSlm:
    default:
        slmSize = slmTotalSize * workGroupCountPerDss;
        break;
    }

    uint32_t actualHwSlmSizeKb = rootDeviceEnvironment.getProductHelper().getActualHwSlmSize(rootDeviceEnvironment);
    slmSize = std::min(slmSize, static_cast<uint32_t>(actualHwSlmSizeKb * MemoryConstants::kiloByte));

    auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    const auto &sizeToPreferredSlmValueArray = releaseHelper->getSizeToPreferredSlmValue();

    uint32_t programmableIdPreferredSlmSize = 0;
    for (auto &range : sizeToPreferredSlmValueArray) {
        if (slmSize <= range.upperLimit) {
            programmableIdPreferredSlmSize = range.valueToProgram;
            break;
        }
    }

    if (debugManager.flags.OverridePreferredSlmAllocationSizePerDss.get() != -1) {
        programmableIdPreferredSlmSize = static_cast<uint32_t>(debugManager.flags.OverridePreferredSlmAllocationSizePerDss.get());
    }

    pInterfaceDescriptor->setPreferredSlmAllocationSize(static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(programmableIdPreferredSlmSize));
}

} // namespace NEO
