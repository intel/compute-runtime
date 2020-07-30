/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/program/program_info.h"

#include <tuple>

namespace NEO {

template <>
bool isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(const ArrayRef<const uint8_t> binary) {
    auto header = Elf::decodeElfFileHeader<Elf::EI_CLASS_64>(binary);
    if (nullptr == header) {
        return false;
    }

    return header->type == NEO::Elf::ET_ZEBIN_EXE;
}

template <>
SingleDeviceBinary unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                                            std::string &outErrReason, std::string &outWarning) {
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(archive, outErrReason, outWarning);
    if (nullptr == elf.elfFileHeader) {
        return {};
    }

    switch (elf.elfFileHeader->type) {
    default:
        outErrReason = "Unhandled elf type";
        return {};
    case NEO::Elf::ET_ZEBIN_EXE:
        break;
    }

    const auto &flags = reinterpret_cast<const NEO::Elf::ZebinTargetFlags &>(elf.elfFileHeader->flags);
    bool validForTarget = flags.machineEntryUsesGfxCoreInsteadOfProductFamily
                              ? (requestedTargetDevice.coreFamily == static_cast<GFXCORE_FAMILY>(elf.elfFileHeader->machine))
                              : (requestedTargetDevice.productFamily == static_cast<PRODUCT_FAMILY>(elf.elfFileHeader->machine));
    validForTarget &= (requestedTargetDevice.maxPointerSizeInBytes == 8U);
    validForTarget &= (0 == flags.validateRevisionId) | ((requestedTargetDevice.stepping >= flags.minHwRevisionId) & (requestedTargetDevice.stepping <= flags.maxHwRevisionId));
    if (false == validForTarget) {
        outErrReason = "Unhandled target device";
        return {};
    }

    SingleDeviceBinary ret;
    ret.deviceBinary = archive;
    ret.format = NEO::DeviceBinaryFormat::Zebin;
    ret.targetDevice = requestedTargetDevice;

    return ret;
}

} // namespace NEO
