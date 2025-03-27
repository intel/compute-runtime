/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/metadata_generation.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"
#include "shared/source/program/kernel_info.h"

#include <string>

namespace NEO {

void populateDefaultMetadata(const ArrayRef<const uint8_t> refBin, size_t kernelMiscInfoOffset, std::vector<NEO::KernelInfo *> &kernelInfos) {
    if (!NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::zebin>(refBin)) {
        return;
    }
    std::string errors{}, warnings{};
    auto zeInfo = Zebin::getZeInfoFromZebin(refBin, errors, warnings);
    auto decodeError = Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(kernelMiscInfoOffset, kernelInfos, zeInfo, errors, warnings);
    if (decodeError != DecodeError::success) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "decodeAndPopulateKernelMiscInfo failed with errors %s and warnings %s\n", errors.c_str(), warnings.c_str());
    }
}

void generateMetadata(std::vector<NEO::KernelInfo *> &kernelInfos) {
    auto ensureTypeNone = [](ArgTypeTraits &typeTraits) -> void {
        typeTraits.typeQualifiers.constQual = false;
        typeTraits.typeQualifiers.pipeQual = false;
        typeTraits.typeQualifiers.restrictQual = false;
        typeTraits.typeQualifiers.unknownQual = false;
        typeTraits.typeQualifiers.volatileQual = false;
    };

    for (const auto &kernelInfo : kernelInfos) {
        if (false == kernelInfo->kernelDescriptor.explicitArgsExtendedMetadata.empty()) {
            continue;
        }
        size_t argIndex = 0u;
        kernelInfo->kernelDescriptor.explicitArgsExtendedMetadata.resize(kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.size());
        for (auto &kernelArg : kernelInfo->kernelDescriptor.payloadMappings.explicitArgs) {
            ArgTypeMetadataExtended argMetadataExtended;
            auto &argTypeTraits = kernelArg.getTraits();
            argMetadataExtended.argName = std::string("arg" + std::to_string(argIndex));

            if (kernelArg.is<ArgDescriptor::argTValue>()) {
                const auto &argAsValue = kernelArg.as<ArgDescValue>(false);
                uint16_t maxSourceOffset = 0u, elemSize = 0u;
                for (const auto &elem : argAsValue.elements) {
                    if (maxSourceOffset <= elem.sourceOffset) {
                        maxSourceOffset = elem.sourceOffset;
                        elemSize = elem.size;
                    }
                }
                if (maxSourceOffset != 0u) {
                    argMetadataExtended.type = std::string("__opaque_var;" + std::to_string(maxSourceOffset + elemSize));
                } else {
                    argMetadataExtended.type = std::string("__opaque;" + std::to_string(elemSize));
                }
                ensureTypeNone(argTypeTraits);
                argTypeTraits.addressQualifier = KernelArgMetadata::AddrPrivate;
                argTypeTraits.accessQualifier = KernelArgMetadata::AccessNone;
            } else if (kernelArg.is<ArgDescriptor::argTPointer>()) {
                const auto &argAsPtr = kernelArg.as<ArgDescPointer>(false);
                argMetadataExtended.type = std::string("__opaque_ptr;" + std::to_string(argAsPtr.pointerSize));
            } else if (kernelArg.is<ArgDescriptor::argTImage>()) {
                const auto &argAsImage = kernelArg.as<ArgDescImage>(false);
                switch (argAsImage.imageType) {
                case NEOImageType::imageTypeBuffer:
                    argMetadataExtended.type = std::string("image1d_buffer_t");
                    break;
                case NEOImageType::imageType1D:
                    argMetadataExtended.type = std::string("image1d_t");
                    break;
                case NEOImageType::imageType1DArray:
                    argMetadataExtended.type = std::string("image1d_array_t");
                    break;
                case NEOImageType::imageType2DArray:
                    argMetadataExtended.type = std::string("image2d_array_t");
                    break;
                case NEOImageType::imageType3D:
                    argMetadataExtended.type = std::string("image3d_t");
                    break;
                case NEOImageType::imageType2DDepth:
                    argMetadataExtended.type = std::string("image2d_depth_t");
                    break;
                case NEOImageType::imageType2DArrayDepth:
                    argMetadataExtended.type = std::string("image2d_array_depth_t");
                    break;
                case NEOImageType::imageType2DMSAA:
                    argMetadataExtended.type = std::string("image2d_msaa_t");
                    break;
                case NEOImageType::imageType2DMSAADepth:
                    argMetadataExtended.type = std::string("image2d_msaa_depth_t");
                    break;
                case NEOImageType::imageType2DArrayMSAA:
                    argMetadataExtended.type = std::string("image2d_array_msaa_t");
                    break;
                case NEOImageType::imageType2DArrayMSAADepth:
                    argMetadataExtended.type = std::string("image2d_array_msaa_depth_t");
                    break;
                default:
                    argMetadataExtended.type = std::string("image2d_t");
                    break;
                }
            } else if (kernelArg.is<ArgDescriptor::argTSampler>()) {
                argMetadataExtended.type = std::string("sampler_t");
            }
            kernelInfo->kernelDescriptor.explicitArgsExtendedMetadata.at(argIndex) = std::move(argMetadataExtended);
            argIndex++;
        }
    }
}

void MetadataGeneration::callPopulateZebinExtendedArgsMetadataOnce(const ArrayRef<const uint8_t> refBin, size_t kernelMiscInfoOffset, std::vector<NEO::KernelInfo *> &kernelInfos) {
    auto extractAndDecodeMetadata = [&]() {
        populateDefaultMetadata(refBin, kernelMiscInfoOffset, kernelInfos);
    };
    std::call_once(extractAndDecodeMetadataOnceFlag, extractAndDecodeMetadata);
}

void MetadataGeneration::callGenerateDefaultExtendedArgsMetadataOnce(std::vector<NEO::KernelInfo *> &kernelInfos) {
    auto generateDefaultMetadata = [&]() {
        generateMetadata(kernelInfos);
    };
    std::call_once(generateDefaultMetadataOnceFlag, generateDefaultMetadata);
}

} // namespace NEO