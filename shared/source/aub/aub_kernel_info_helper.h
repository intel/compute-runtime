/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/aub/aub_center.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/kernel/implicit_args_base.h"

#include "implicit_args.h"

#include <cstdint>
#include <sstream>
#include <string>

namespace NEO {
namespace AubComment {

inline constexpr const char *kernelInfoToken = "<KernelInfo>\n";
inline constexpr const char *exportedSymbolsToken = "<ExportedSymbols>\n";
inline constexpr const char *buildOptionsToken = "<BuildOptions>\n";
inline constexpr const char *zeInfoToken = "<Zeinfo>\n";
inline constexpr const char *implicitArgsToken = "<ImplicitArgs>\n";
inline constexpr const char *kernelUsesImplicitArgsToken = "Kernel uses implicit args\n";

inline std::string formatEntry(const std::string &name, uint64_t address, uint64_t size) {
    std::ostringstream os;
    os << "name: " << name << "\n"
       << "address: 0x" << std::hex << address << "\n"
       << "size: " << std::dec << size << "\n";
    return os.str();
}

inline std::string printImplicitArgsLayouts(uint32_t implicitArgsVersion) {

    std::ostringstream os;
    os << implicitArgsToken;

    switch (implicitArgsVersion) {
    case 0:
        os << ImplicitArgsV0::getStructAsString() << "\n";
        os << "Aligned size: " << static_cast<uint32_t>(ImplicitArgsV0::getAlignedSize()) << "\n";
        break;
    case 1:
        os << ImplicitArgsV1::getStructAsString() << "\n";
        os << "Aligned size: " << static_cast<uint32_t>(ImplicitArgsV1::getAlignedSize()) << "\n";
        break;
    case 2:
        os << ImplicitArgsV2::getStructAsString() << "\n";
        os << "Aligned size: " << static_cast<uint32_t>(ImplicitArgsV2::getAlignedSize()) << "\n";
        break;
    default:
        UNRECOVERABLE_IF(true);
    }

    return os.str();
}

template <typename ContainerT>
inline void dumpKernelInfoToAubComments(bool useFullAddress,
                                        AubCenter *aubCenter,
                                        ContainerT &kernelIsaInfos,
                                        const Linker::RelocatedSymbolsMap &symbols,
                                        const std::string &buildOptions,
                                        ArrayRef<const uint8_t> deviceBinary,
                                        uint32_t implicitArgsVersion) {

    bool implicitArgsUsed = false;

    std::string comments;
    comments.append(kernelInfoToken);

    for (const auto &data : kernelIsaInfos) {
        auto isaAllocation = data->getIsaGraphicsAllocation();
        if (!isaAllocation) {
            continue;
        }

        auto isaGpuAddress = useFullAddress ? isaAllocation->getGpuAddress() : isaAllocation->getGpuAddressToPatch();
        isaGpuAddress += data->getIsaOffsetInParentAllocation();
        comments.append(formatEntry(data->getDescriptor().kernelMetadata.kernelName, isaGpuAddress, static_cast<uint32_t>(data->getIsaSize())));

        if (data->getDescriptor().kernelAttributes.flags.requiresImplicitArgs) {
            implicitArgsUsed = true;
            comments.append(kernelUsesImplicitArgsToken);
        }
        comments.append("\n");
    }

    if (!symbols.empty()) {
        comments.append(exportedSymbolsToken);
        for (const auto &[name, symbol] : symbols) {
            comments.append(formatEntry(name, symbol.gpuAddress, symbol.symbol.size));
            comments.append("\n");
        }
    }

    if (!buildOptions.empty()) {
        comments.append(buildOptionsToken + buildOptions + "\n\n");
    }

    if (!deviceBinary.empty()) {
        std::string errors;
        std::string warnings;
        auto zeInfoYaml = NEO::Zebin::getZeInfoFromZebin(deviceBinary, errors, warnings);

        if (!zeInfoYaml.empty()) {
            comments.append(zeInfoToken);
            comments.append(zeInfoYaml.begin(), zeInfoYaml.size());
        }
    }

    if (aubCenter && aubCenter->getAubManager()) {
        if (implicitArgsUsed) {
            aubCenter->addImplicitArgsInfoToAubComments(implicitArgsVersion);
        }
        aubCenter->getAubManager()->addComment(comments.c_str());
    }
}

} // namespace AubComment
} // namespace NEO
