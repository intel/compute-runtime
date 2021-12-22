/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"

#include "os_inc.h"

#include <cstdint>

namespace NEO {

const char *getBuiltinAsString(EBuiltInOps::Type builtin) {
    const char *builtinString = getAdditionalBuiltinAsString(builtin);
    if (builtinString) {
        return builtinString;
    }
    switch (builtin) {
    default:
        return getUnknownBuiltinAsString(builtin);
    case EBuiltInOps::AuxTranslation:
        return "aux_translation.builtin_kernel";
    case EBuiltInOps::CopyBufferToBuffer:
        return "copy_buffer_to_buffer.builtin_kernel";
    case EBuiltInOps::CopyBufferToBufferStateless:
        return "copy_buffer_to_buffer_stateless.builtin_kernel";
    case EBuiltInOps::CopyBufferRect:
        return "copy_buffer_rect.builtin_kernel";
    case EBuiltInOps::CopyBufferRectStateless:
        return "copy_buffer_rect_stateless.builtin_kernel";
    case EBuiltInOps::FillBuffer:
        return "fill_buffer.builtin_kernel";
    case EBuiltInOps::FillBufferStateless:
        return "fill_buffer_stateless.builtin_kernel";
    case EBuiltInOps::CopyBufferToImage3d:
        return "copy_buffer_to_image3d.builtin_kernel";
    case EBuiltInOps::CopyBufferToImage3dStateless:
        return "copy_buffer_to_image3d_stateless.builtin_kernel";
    case EBuiltInOps::CopyImage3dToBuffer:
        return "copy_image3d_to_buffer.builtin_kernel";
    case EBuiltInOps::CopyImage3dToBufferStateless:
        return "copy_image3d_to_buffer_stateless.builtin_kernel";
    case EBuiltInOps::CopyImageToImage1d:
        return "copy_image_to_image1d.builtin_kernel";
    case EBuiltInOps::CopyImageToImage2d:
        return "copy_image_to_image2d.builtin_kernel";
    case EBuiltInOps::CopyImageToImage3d:
        return "copy_image_to_image3d.builtin_kernel";
    case EBuiltInOps::FillImage1d:
        return "fill_image1d.builtin_kernel";
    case EBuiltInOps::FillImage2d:
        return "fill_image2d.builtin_kernel";
    case EBuiltInOps::FillImage3d:
        return "fill_image3d.builtin_kernel";
    case EBuiltInOps::QueryKernelTimestamps:
        return "copy_kernel_timestamps.builtin_kernel";
    };
}

BuiltinResourceT createBuiltinResource(const char *ptr, size_t size) {
    return BuiltinResourceT(ptr, ptr + size);
}

BuiltinResourceT createBuiltinResource(const BuiltinResourceT &r) {
    return BuiltinResourceT(r);
}

std::string createBuiltinResourceName(EBuiltInOps::Type builtin, const std::string &extension,
                                      const std::string &platformName, uint32_t deviceRevId) {
    std::string ret;
    if (platformName.size() > 0) {
        ret = platformName;
        ret += "_" + std::to_string(deviceRevId);
        ret += "_";
    }
    if (extension == ".bin") {
        ret += ApiSpecificConfig::getBindlessConfiguration() ? "bindless_" : "bindful_";
    }
    ret += getBuiltinAsString(builtin);

    if (extension.size() > 0) {
        ret += extension;
    }

    return ret;
}

std::string joinPath(const std::string &lhs, const std::string &rhs) {
    if (lhs.size() == 0) {
        return rhs;
    }

    if (rhs.size() == 0) {
        return lhs;
    }

    if (*lhs.rbegin() == PATH_SEPARATOR) {
        return lhs + rhs;
    }

    return lhs + PATH_SEPARATOR + rhs;
}

std::string getDriverInstallationPath() {
    return "";
}

BuiltinResourceT Storage::load(const std::string &resourceName) {
    return loadImpl(joinPath(rootPath, resourceName));
}

BuiltinResourceT FileStorage::loadImpl(const std::string &fullResourceName) {
    BuiltinResourceT ret;

    std::ifstream f{fullResourceName, std::ios::in | std::ios::binary | std::ios::ate};
    auto end = f.tellg();
    f.seekg(0, std::ios::beg);
    auto beg = f.tellg();
    auto s = end - beg;
    ret.resize(static_cast<size_t>(s));
    f.read(ret.data(), s);
    return ret;
}

const BuiltinResourceT *EmbeddedStorageRegistry::get(const std::string &name) const {
    auto it = resources.find(name);
    if (resources.end() == it) {
        return nullptr;
    }

    return &it->second;
}

BuiltinResourceT EmbeddedStorage::loadImpl(const std::string &fullResourceName) {
    auto *constResource = EmbeddedStorageRegistry::getInstance().get(fullResourceName);
    if (constResource == nullptr) {
        BuiltinResourceT ret;
        return ret;
    }

    return createBuiltinResource(*constResource);
}

BuiltinsLib::BuiltinsLib() {
    allStorages.push_back(std::unique_ptr<Storage>(new EmbeddedStorage("")));
    allStorages.push_back(std::unique_ptr<Storage>(new FileStorage(getDriverInstallationPath())));
}

BuiltinCode BuiltinsLib::getBuiltinCode(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
    std::lock_guard<std::mutex> lockRaii{mutex};

    BuiltinResourceT bc;
    BuiltinCode::ECodeType usedCodetType = BuiltinCode::ECodeType::INVALID;

    if (requestedCodeType == BuiltinCode::ECodeType::Any) {
        uint32_t codeType = static_cast<uint32_t>(BuiltinCode::ECodeType::Binary);
        if (DebugManager.flags.RebuildPrecompiledKernels.get()) {
            codeType = static_cast<uint32_t>(BuiltinCode::ECodeType::Source);
        }
        for (uint32_t e = static_cast<uint32_t>(BuiltinCode::ECodeType::COUNT);
             codeType != e; ++codeType) {
            bc = getBuiltinResource(builtin, static_cast<BuiltinCode::ECodeType>(codeType), device);
            if (bc.size() > 0) {
                usedCodetType = static_cast<BuiltinCode::ECodeType>(codeType);
                break;
            }
        }
    } else {
        bc = getBuiltinResource(builtin, requestedCodeType, device);
        usedCodetType = requestedCodeType;
    }

    BuiltinCode ret;
    std::swap(ret.resource, bc);
    ret.type = usedCodetType;
    ret.targetDevice = &device;

    return ret;
}

BuiltinResourceT BuiltinsLib::getBuiltinResource(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
    BuiltinResourceT bc;
    auto &hwInfo = device.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    std::string resourceNameGeneric = createBuiltinResourceName(builtin, BuiltinCode::getExtension(requestedCodeType));
    std::string resourceNameForPlatformType = createBuiltinResourceName(builtin, BuiltinCode::getExtension(requestedCodeType),
                                                                        getFamilyNameWithType(hwInfo),
                                                                        hwHelper.getDefaultRevisionId(hwInfo));
    std::string resourceNameForPlatformTypeAndStepping = createBuiltinResourceName(builtin, BuiltinCode::getExtension(requestedCodeType),
                                                                                   getFamilyNameWithType(hwInfo),
                                                                                   hwInfo.platform.usRevId);

    StackVec<const std::string *, 3> resourcesToLookup;
    resourcesToLookup.push_back(&resourceNameForPlatformTypeAndStepping);
    if (BuiltinCode::ECodeType::Binary != requestedCodeType || !hwHelper.isRevisionSpecificBinaryBuiltinRequired()) {
        resourcesToLookup.push_back(&resourceNameForPlatformType);
        resourcesToLookup.push_back(&resourceNameGeneric);
    }
    for (auto &rn : resourcesToLookup) { // first look for dedicated version, only fallback to generic one
        for (auto &s : allStorages) {
            UNRECOVERABLE_IF(!rn);
            bc = s.get()->load(*rn);
            if (bc.size() != 0) {
                return bc;
            }
        }
    }
    return bc;
}

} // namespace NEO
