/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cstdint>
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "os_inc.h"

namespace OCLRT {

const char *getBuiltinAsString(EBuiltInOps builtin) {
    switch (builtin) {
    default:
        return "unknown";
    case EBuiltInOps::CopyBufferToBuffer:
        return "copy_buffer_to_buffer.igdrcl_built_in";
    case EBuiltInOps::CopyBufferRect:
        return "copy_buffer_rect.igdrcl_built_in";
    case EBuiltInOps::FillBuffer:
        return "fill_buffer.igdrcl_built_in";
    case EBuiltInOps::CopyBufferToImage3d:
        return "copy_buffer_to_image3d.igdrcl_built_in";
    case EBuiltInOps::CopyImage3dToBuffer:
        return "copy_image3d_to_buffer.igdrcl_built_in";
    case EBuiltInOps::CopyImageToImage1d:
        return "copy_image_to_image1d.igdrcl_built_in";
    case EBuiltInOps::CopyImageToImage2d:
        return "copy_image_to_image2d.igdrcl_built_in";
    case EBuiltInOps::CopyImageToImage3d:
        return "copy_image_to_image3d.igdrcl_built_in";
    case EBuiltInOps::FillImage1d:
        return "fill_image1d.igdrcl_built_in";
    case EBuiltInOps::FillImage2d:
        return "fill_image2d.igdrcl_built_in";
    case EBuiltInOps::FillImage3d:
        return "fill_image3d.igdrcl_built_in";
    case EBuiltInOps::VmeBlockMotionEstimateIntel:
        return "vme_block_motion_estimate_intel.igdrcl_built_in";
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel:
        return "vme_block_advanced_motion_estimate_check_intel.igdrcl_built_in";
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel:
        return "vme_block_advanced_motion_estimate_bidirectional_check_intel";
    case EBuiltInOps::Scheduler:
        return "scheduler.igdrcl_built_in";
    };
}

BuiltinResourceT createBuiltinResource(const char *ptr, size_t size) {
    return BuiltinResourceT(ptr, ptr + size);
}

BuiltinResourceT createBuiltinResource(const BuiltinResourceT &r) {
    return BuiltinResourceT(r);
}

std::string createBuiltinResourceName(EBuiltInOps builtin, const std::string &extension,
                                      const std::string &platformName, uint32_t deviceRevId) {
    std::string ret;
    if (platformName.size() > 0) {
        ret = platformName;
        ret += "_" + std::to_string(deviceRevId);
        ret += "_";
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

BuiltinCode BuiltinsLib::getBuiltinCode(EBuiltInOps builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
    std::lock_guard<std::mutex> lockRaii{mutex};

    BuiltinResourceT bc;
    BuiltinCode::ECodeType usedCodetType = BuiltinCode::ECodeType::INVALID;
    if (requestedCodeType == BuiltinCode::ECodeType::Any) {
        for (uint32_t codeType = static_cast<uint32_t>(BuiltinCode::ECodeType::Binary), e = static_cast<uint32_t>(BuiltinCode::ECodeType::COUNT);
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

std::unique_ptr<Program> BuiltinsLib::createProgramFromCode(const BuiltinCode &bc, Context &context, Device &device) {
    std::unique_ptr<Program> ret;
    const char *data = bc.resource.data();
    size_t dataLen = bc.resource.size();
    cl_int err = 0;
    switch (bc.type) {
    default:
        break;
    case BuiltinCode::ECodeType::Source:
    case BuiltinCode::ECodeType::Intermediate:
        ret.reset(Program::create(data, &context, device, &err));
        break;
    case BuiltinCode::ECodeType::Binary:
        ret.reset(Program::createFromGenBinary(&context, data, dataLen, nullptr));
        break;
    }
    return ret;
}

BuiltinResourceT BuiltinsLib::getBuiltinResource(EBuiltInOps builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
    BuiltinResourceT bc;
    std::string resourceNameGeneric = createBuiltinResourceName(builtin, BuiltinCode::getExtension(requestedCodeType));
    std::string resourceNameForPlatformType = createBuiltinResourceName(builtin, BuiltinCode::getExtension(requestedCodeType), device.getFamilyNameWithType());
    std::string resourceNameForPlatformTypeAndStepping = createBuiltinResourceName(builtin, BuiltinCode::getExtension(requestedCodeType), device.getFamilyNameWithType(),
                                                                                   device.getHardwareInfo().pPlatform->usRevId);

    for (auto &rn : {resourceNameForPlatformTypeAndStepping, resourceNameForPlatformType, resourceNameGeneric}) { // first look for dedicated version, only fallback to generic one
        for (auto &s : allStorages) {
            bc = s.get()->load(rn);
            if (bc.size() != 0) {
                return bc;
            }
        }
    }
    return bc;
}

} // namespace OCLRT
