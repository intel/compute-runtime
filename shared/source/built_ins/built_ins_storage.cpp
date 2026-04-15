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
#include "shared/source/utilities/io_functions.h"

#include "os_inc.h"

#include <cstdint>
#include <memory>
#include <sstream>

namespace NEO {

BuiltIn::Resource BuiltIn::createResource(const char *ptr, size_t size) {
    return BuiltIn::Resource(ptr, ptr + size);
}

BuiltIn::Resource BuiltIn::createResource(const BuiltIn::Resource &r) {
    return BuiltIn::Resource(r);
}

std::string BuiltIn::createResourceName(BuiltIn::BaseKernel kernel, const std::string &extension) {
    return std::string(BuiltIn::getAsString(kernel)) + extension;
}

StackVec<std::string, 3> BuiltIn::getResourceNames(BuiltIn::BaseKernel kernel, const BuiltIn::AddressingMode &mode, BuiltIn::CodeType type, const Device &device) {
    auto &hwInfo = device.getHardwareInfo();

    const auto deviceIp = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);
    const auto extension = BuiltIn::Code::getExtension(type);

    // Source files use the base name without the suffix.

    std::string builtinFilename = BuiltIn::getAsString(kernel);
    std::string modePrefix;
    if (type != BuiltIn::CodeType::source) {
        modePrefix = mode.toString();
    }

    auto createResourceNameStr = [](ConstStringRef deviceIpPath, std::string_view addressingModePrefix, std::string_view builtinFilenameStr, std::string_view extensionStr) {
        std::ostringstream outResourceName;
        if (false == deviceIpPath.empty()) {
            outResourceName << deviceIpPath.str() << "_";
        }
        outResourceName << addressingModePrefix << builtinFilenameStr << extensionStr;
        return outResourceName.str();
    };

    StackVec<std::string, 3> resourcesToLookup = {};
    resourcesToLookup.push_back(createResourceNameStr(deviceIp, modePrefix, builtinFilename, extension));

    if (BuiltIn::CodeType::binary != type) {
        resourcesToLookup.push_back(createResourceNameStr("", modePrefix, builtinFilename, extension));
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

    std::unique_ptr<FILE, decltype(NEO::IoFunctions::fclosePtr)> fp(
        NEO::IoFunctions::fopenPtr(fullResourceName.c_str(), "rb"),
        NEO::IoFunctions::fclosePtr);
    if (fp == nullptr) {
        return ret;
    }
    if (NEO::IoFunctions::fseekPtr(fp.get(), 0, SEEK_END) != 0) {
        return ret;
    }
    auto fileSize = NEO::IoFunctions::ftellPtr(fp.get());
    if (fileSize < 0) {
        return ret;
    }
    if (NEO::IoFunctions::fseekPtr(fp.get(), 0, SEEK_SET) != 0) {
        return ret;
    }
    auto size = static_cast<size_t>(fileSize);
    ret.resize(size);
    auto bytesRead = NEO::IoFunctions::freadPtr(ret.data(), sizeof(char), size, fp.get());
    if (bytesRead != size) {
        return BuiltIn::Resource{};
    }
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

BuiltIn::Code BuiltIn::ResourceLoader::getBuiltinCode(BuiltIn::BaseKernel kernel, const BuiltIn::AddressingMode &mode, BuiltIn::CodeType requestedCodeType, Device &device) {
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
            bc = getBuiltinResource(kernel, mode, static_cast<BuiltIn::CodeType>(codeType), device);
            if (bc.size() > 0) {
                usedCodetType = static_cast<BuiltIn::CodeType>(codeType);
                break;
            }
        }
    } else {
        bc = getBuiltinResource(kernel, mode, requestedCodeType, device);
        usedCodetType = requestedCodeType;
    }

    BuiltIn::Code ret;
    std::swap(ret.resource, bc);
    ret.type = usedCodetType;
    ret.targetDevice = &device;

    return ret;
}

BuiltIn::Resource BuiltIn::ResourceLoader::getBuiltinResource(BuiltIn::BaseKernel kernel, const BuiltIn::AddressingMode &mode, BuiltIn::CodeType requestedCodeType, Device &device) {
    BuiltIn::Resource builtinResource;
    auto resourcesToLookup = BuiltIn::getResourceNames(kernel, mode, requestedCodeType, device);
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
