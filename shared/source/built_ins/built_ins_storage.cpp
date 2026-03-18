/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/path.h"

#include "os_inc.h"

#include <cstdint>
#include <fstream>
#include <sstream>

namespace NEO {

const char *BuiltIn::getAsString(BuiltIn::Group builtInGroup) {
    switch (builtInGroup) {
    default:
        return "unknown";
    case BuiltIn::Group::auxTranslation:
        return "aux_translation.builtin_kernel";
    case BuiltIn::Group::copyBufferToBuffer:
        return "copy_buffer_to_buffer.builtin_kernel";
    case BuiltIn::Group::copyBufferToBufferStateless:
    case BuiltIn::Group::copyBufferToBufferWideStateless:
    case BuiltIn::Group::copyBufferToBufferStatelessHeapless:
    case BuiltIn::Group::copyBufferToBufferWideStatelessHeapless:
        return "copy_buffer_to_buffer_stateless.builtin_kernel";
    case BuiltIn::Group::copyBufferRect:
        return "copy_buffer_rect.builtin_kernel";
    case BuiltIn::Group::copyBufferRectStateless:
    case BuiltIn::Group::copyBufferRectWideStateless:
    case BuiltIn::Group::copyBufferRectStatelessHeapless:
    case BuiltIn::Group::copyBufferRectWideStatelessHeapless:
        return "copy_buffer_rect_stateless.builtin_kernel";
    case BuiltIn::Group::fillBuffer:
        return "fill_buffer.builtin_kernel";
    case BuiltIn::Group::fillBufferStateless:
    case BuiltIn::Group::fillBufferWideStateless:
    case BuiltIn::Group::fillBufferStatelessHeapless:
    case BuiltIn::Group::fillBufferWideStatelessHeapless:
        return "fill_buffer_stateless.builtin_kernel";
    case BuiltIn::Group::copyBufferToImage3d:
        return "copy_buffer_to_image3d.builtin_kernel";
    case BuiltIn::Group::copyBufferToImage3dStateless:
    case BuiltIn::Group::copyBufferToImage3dWideStateless:
    case BuiltIn::Group::copyBufferToImage3dStatelessHeapless:
    case BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless:
        return "copy_buffer_to_image3d_stateless.builtin_kernel";
    case BuiltIn::Group::copyImage3dToBuffer:
        return "copy_image3d_to_buffer.builtin_kernel";
    case BuiltIn::Group::copyImage3dToBufferStateless:
    case BuiltIn::Group::copyImage3dToBufferWideStateless:
    case BuiltIn::Group::copyImage3dToBufferStatelessHeapless:
    case BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless:
        return "copy_image3d_to_buffer_stateless.builtin_kernel";
    case BuiltIn::Group::copyImageToImage1d:
    case BuiltIn::Group::copyImageToImage1dHeapless:
        return "copy_image_to_image1d.builtin_kernel";
    case BuiltIn::Group::copyImageToImage2d:
    case BuiltIn::Group::copyImageToImage2dHeapless:
        return "copy_image_to_image2d.builtin_kernel";
    case BuiltIn::Group::copyImageToImage3d:
    case BuiltIn::Group::copyImageToImage3dHeapless:
        return "copy_image_to_image3d.builtin_kernel";
    case BuiltIn::Group::fillImage1d:
    case BuiltIn::Group::fillImage1dHeapless:
        return "fill_image1d.builtin_kernel";
    case BuiltIn::Group::fillImage2d:
    case BuiltIn::Group::fillImage2dHeapless:
        return "fill_image2d.builtin_kernel";
    case BuiltIn::Group::fillImage3d:
    case BuiltIn::Group::fillImage3dHeapless:
        return "fill_image3d.builtin_kernel";
    case BuiltIn::Group::queryKernelTimestamps:
    case BuiltIn::Group::queryKernelTimestampsStateless:
    case BuiltIn::Group::queryKernelTimestampsStatelessHeapless:
        return "copy_kernel_timestamps.builtin_kernel";
    case BuiltIn::Group::fillImage1dBuffer:
    case BuiltIn::Group::fillImage1dBufferHeapless:
        return "fill_image1d_buffer.builtin_kernel";
    };
}

BuiltIn::Resource BuiltIn::createResource(const char *ptr, size_t size) {
    return BuiltIn::Resource(ptr, ptr + size);
}

BuiltIn::Resource BuiltIn::createResource(const BuiltIn::Resource &r) {
    return BuiltIn::Resource(r);
}

std::string BuiltIn::createResourceName(BuiltIn::Group builtInGroup, const std::string &extension) {
    return BuiltIn::getAsString(builtInGroup) + extension;
}

StackVec<std::string, 3> BuiltIn::getResourceNames(BuiltIn::Group builtInGroup, BuiltIn::CodeType type, const Device &device) {
    auto &hwInfo = device.getHardwareInfo();
    auto &productHelper = device.getRootDeviceEnvironment().getHelper<ProductHelper>();

    auto createDeviceIdFilenameComponent = [](const NEO::HardwareIpVersion &hwIpVersion) {
        std::ostringstream deviceId;
        deviceId << hwIpVersion.architecture << "_" << hwIpVersion.release << "_" << hwIpVersion.revision;
        return deviceId.str();
    };
    const auto deviceIp = createDeviceIdFilenameComponent(hwInfo.ipVersion);
    const auto builtinFilename = BuiltIn::getAsString(builtInGroup);
    const auto extension = BuiltIn::Code::getExtension(type);

    std::string_view addressingModePrefix;
    const bool builtInUsesWideStatelessAddressing = BuiltIn::isWideStateless(builtInGroup);
    if (type == BuiltIn::CodeType::binary) {
        const bool heaplessEnabled = BuiltIn::isHeapless(builtInGroup);
        const bool requiresStatelessAddressing = (false == productHelper.isStatefulAddressingModeSupported());
        const bool builtInUsesStatelessAddressing = BuiltIn::isStateless(builtInGroup) || builtInUsesWideStatelessAddressing;
        if (heaplessEnabled) {
            addressingModePrefix = builtInUsesWideStatelessAddressing ? "wide_stateless_heapless_" : "stateless_heapless_";
        } else if (builtInUsesStatelessAddressing || requiresStatelessAddressing) {
            addressingModePrefix = builtInUsesWideStatelessAddressing ? "wide_stateless_" : "stateless_";
        } else if (ApiSpecificConfig::getBindlessMode(device)) {
            addressingModePrefix = "bindless_";
        } else {
            addressingModePrefix = "bindful_";
        }
    } else if (type == BuiltIn::CodeType::intermediate && builtInUsesWideStatelessAddressing) {
        addressingModePrefix = "wide_stateless_";
    }

    auto createResourceName = [](ConstStringRef deviceIpPath, std::string_view addressingModePrefix, std::string_view builtinFilename, std::string_view extension) {
        std::ostringstream outResourceName;
        if (false == deviceIpPath.empty()) {
            outResourceName << deviceIpPath.str() << "_";
        }
        outResourceName << addressingModePrefix << builtinFilename << extension;
        return outResourceName.str();
    };
    StackVec<std::string, 3> resourcesToLookup = {};
    resourcesToLookup.push_back(createResourceName(deviceIp, addressingModePrefix, builtinFilename, extension));

    if (BuiltIn::CodeType::binary != type) {
        resourcesToLookup.push_back(createResourceName("", addressingModePrefix, builtinFilename, extension));
    }
    return resourcesToLookup;
}

std::string getDriverInstallationPath() {
    return "";
}

BuiltIn::Resource BuiltIn::Storage::load(const std::string &resourceName) {
    return loadImpl(joinPath(rootPath, resourceName));
}

BuiltIn::Resource BuiltIn::FileStorage::loadImpl(const std::string &fullResourceName) {
    BuiltIn::Resource ret;

    std::ifstream f{fullResourceName, std::ios::in | std::ios::binary | std::ios::ate};
    auto end = f.tellg();
    f.seekg(0, std::ios::beg);
    auto beg = f.tellg();
    auto s = end - beg;
    ret.resize(static_cast<size_t>(s));
    f.read(ret.data(), s);
    return ret;
}

const BuiltIn::Resource *BuiltIn::EmbeddedStorageRegistry::get(const std::string &name) const {
    auto it = resources.find(name);
    if (resources.end() == it) {
        return nullptr;
    }

    return &it->second;
}

BuiltIn::Resource BuiltIn::EmbeddedStorage::loadImpl(const std::string &fullResourceName) {
    auto *constResource = BuiltIn::EmbeddedStorageRegistry::getInstance().get(fullResourceName);
    if (constResource == nullptr) {
        BuiltIn::Resource ret;
        return ret;
    }

    return BuiltIn::createResource(*constResource);
}

BuiltIn::ResourceLoader::ResourceLoader() {
    allStorages.push_back(std::unique_ptr<BuiltIn::Storage>(new BuiltIn::EmbeddedStorage("")));
    allStorages.push_back(std::unique_ptr<BuiltIn::Storage>(new BuiltIn::FileStorage(getDriverInstallationPath())));
}

BuiltIn::Code BuiltIn::ResourceLoader::getBuiltinCode(BuiltIn::Group builtInGroup, BuiltIn::CodeType requestedCodeType, Device &device) {
    std::lock_guard<std::mutex> lockRaii{mutex};

    BuiltIn::Resource bc;
    BuiltIn::CodeType usedCodetType = BuiltIn::CodeType::invalid;

    if (requestedCodeType == BuiltIn::CodeType::any) {
        uint32_t codeType = static_cast<uint32_t>(BuiltIn::CodeType::binary);
        bool requiresRebuild = !device.getExecutionEnvironment()->isOneApiPvcWaEnv();
        if (requiresRebuild || debugManager.flags.RebuildPrecompiledKernels.get()) {
            codeType = static_cast<uint32_t>(BuiltIn::CodeType::source);
        }
        for (uint32_t e = static_cast<uint32_t>(BuiltIn::CodeType::count);
             codeType != e; ++codeType) {
            bc = getBuiltinResource(builtInGroup, static_cast<BuiltIn::CodeType>(codeType), device);
            if (bc.size() > 0) {
                usedCodetType = static_cast<BuiltIn::CodeType>(codeType);
                break;
            }
        }
    } else {
        bc = getBuiltinResource(builtInGroup, requestedCodeType, device);
        usedCodetType = requestedCodeType;
    }

    BuiltIn::Code ret;
    std::swap(ret.resource, bc);
    ret.type = usedCodetType;
    ret.targetDevice = &device;

    return ret;
}

BuiltIn::Resource BuiltIn::ResourceLoader::getBuiltinResource(BuiltIn::Group builtInGroup, BuiltIn::CodeType requestedCodeType, Device &device) {
    BuiltIn::Resource builtinResource;
    auto resourcesToLookup = BuiltIn::getResourceNames(builtInGroup, requestedCodeType, device);
    for (auto &resourceName : resourcesToLookup) {
        for (auto &storage : allStorages) {
            builtinResource = storage->load(resourceName);
            if (builtinResource.size() != 0) {
                return builtinResource;
            }
        }
    }
    return builtinResource;
}

} // namespace NEO
