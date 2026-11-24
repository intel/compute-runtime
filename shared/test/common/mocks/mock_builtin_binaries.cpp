/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_builtin_binaries.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/registry/built_ins_registry.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/device_binary_format/zebin/zeinfo.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/mocks/packaged_builtin_resources.inl"

#include <algorithm>
#include <array>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace NEO {
namespace MockBuiltinBinaries {

namespace {

struct KernelArg {
    enum class Kind {
        globalPointer,
        image,
        value
    };

    Kind kind = Kind::value;
    std::string name;
    std::string typeName;
    uint32_t size = 0;
    bool isConst = false;
    bool readOnly = false;
    bool writeOnly = false;
};

struct KernelSignature {
    std::string name;
    std::vector<KernelArg> args;
};

std::string stripComments(std::string_view source) {
    std::string result;
    result.reserve(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        if (source.compare(i, 2, "//") == 0) {
            i = source.find('\n', i);
            if (i == std::string_view::npos) {
                break;
            }
            result += '\n';
        } else if (source.compare(i, 2, "/*") == 0) {
            i = source.find("*/", i + 2);
            if (i == std::string_view::npos) {
                break;
            }
            ++i;
            result += ' ';
        } else {
            result += source[i];
        }
    }
    return result;
}

std::map<std::string, std::string> parseTypeDefinitions(ConstStringRef typeDefinitions) {
    std::map<std::string, std::string> definitions;
    std::istringstream stream{typeDefinitions.str()};
    std::string option;
    while (stream >> option) {
        constexpr std::string_view definePrefix = "-D";
        auto separator = option.find('=');
        if (!option.starts_with(definePrefix) || separator == std::string::npos) {
            continue;
        }
        definitions[option.substr(definePrefix.size(), separator - definePrefix.size())] = option.substr(separator + 1);
    }
    return definitions;
}

std::optional<uint32_t> getValueTypeSize(const std::string &typeName) {
    static const std::map<std::string, uint32_t> scalarSizes = {
        {"char", 1}, {"uchar", 1}, {"short", 2}, {"ushort", 2}, {"half", 2}, {"int", 4}, {"uint", 4}, {"float", 4}, {"long", 8}, {"ulong", 8}, {"double", 8}};

    auto vectorWidthPosition = typeName.find_first_of("0123456789");
    auto scalarSize = scalarSizes.find(typeName.substr(0, vectorWidthPosition));
    if (scalarSize == scalarSizes.end()) {
        return std::nullopt;
    }
    if (vectorWidthPosition == std::string::npos) {
        return scalarSize->second;
    }
    auto vectorWidth = typeName.substr(vectorWidthPosition);
    static const std::map<std::string, uint32_t> vectorSizes = {{"2", 2}, {"3", 4}, {"4", 4}, {"8", 8}, {"16", 16}};
    auto elementCount = vectorSizes.find(vectorWidth);
    if (elementCount == vectorSizes.end()) {
        return std::nullopt;
    }
    return scalarSize->second * elementCount->second;
}

const char *getImageType(const std::string &typeName) {
    namespace ImageType = Zebin::ZeInfo::Tags::Kernel::PayloadArgument::ImageType;
    static const std::map<std::string, ConstStringRef> imageTypes = {
        {"image1d_t", ImageType::imageType1D}, {"image1d_buffer_t", ImageType::imageTypeBuffer}, {"image1d_array_t", ImageType::imageType1DArray}, {"image2d_t", ImageType::imageType2D}, {"image2d_array_t", ImageType::imageType2DArray}, {"image3d_t", ImageType::imageType3D}};
    auto imageType = imageTypes.find(typeName);
    return imageType == imageTypes.end() ? nullptr : imageType->second.data();
}

std::optional<KernelArg> parseKernelArg(const std::string &declaration, const std::map<std::string, std::string> &typeDefinitions, std::string &outError) {
    std::string spaced;
    for (auto character : declaration) {
        if (character == '*') {
            spaced += " * ";
        } else {
            spaced += character;
        }
    }

    KernelArg arg;
    bool isPointer = false;
    bool isGlobal = false;
    std::vector<std::string> identifiers;
    std::istringstream stream(spaced);
    std::string token;
    while (stream >> token) {
        if (token == "*") {
            isPointer = true;
        } else if (token == "__global" || token == "global") {
            isGlobal = true;
        } else if (token == "const") {
            arg.isConst = true;
        } else if (token == "__read_only" || token == "read_only") {
            arg.readOnly = true;
        } else if (token == "__write_only" || token == "write_only") {
            arg.writeOnly = true;
        } else if (token == "__read_write" || token == "read_write" || token == "restrict" || token == "__restrict" || token == "volatile") {
            continue;
        } else {
            identifiers.push_back(token);
        }
    }

    if (identifiers.size() != 2) {
        outError = "unsupported kernel argument declaration: " + declaration;
        return std::nullopt;
    }
    arg.name = identifiers[1];
    arg.typeName = identifiers[0];
    if (auto definition = typeDefinitions.find(arg.typeName); definition != typeDefinitions.end()) {
        arg.typeName = definition->second;
    }

    if (isPointer) {
        if (!isGlobal) {
            outError = "only global pointers are supported: " + declaration;
            return std::nullopt;
        }
        arg.kind = KernelArg::Kind::globalPointer;
        arg.typeName += "*";
        arg.size = sizeof(uint64_t);
        return arg;
    }
    if (getImageType(arg.typeName) != nullptr) {
        arg.kind = KernelArg::Kind::image;
        arg.size = sizeof(uint64_t);
        return arg;
    }
    auto valueSize = getValueTypeSize(arg.typeName);
    if (!valueSize) {
        outError = "unsupported kernel argument type: " + declaration;
        return std::nullopt;
    }
    arg.kind = KernelArg::Kind::value;
    arg.size = *valueSize;
    return arg;
}

const char *getAccessType(const KernelArg &arg) {
    if (arg.readOnly || (arg.kind == KernelArg::Kind::globalPointer && arg.isConst)) {
        return "readonly";
    }
    if (arg.writeOnly) {
        return "writeonly";
    }
    return "readwrite";
}

const char *getAccessQualifier(const KernelArg &arg) {
    if (arg.kind != KernelArg::Kind::image) {
        return "NONE";
    }
    if (arg.readOnly) {
        return "__read_only";
    }
    if (arg.writeOnly) {
        return "__write_only";
    }
    return "__read_write";
}

using YamlFields = std::vector<std::pair<std::string_view, std::string>>;

void writeListEntry(std::ostringstream &yaml, std::string_view indent, const YamlFields &fields) {
    bool firstField = true;
    for (const auto &[key, value] : fields) {
        yaml << indent << (firstField ? "- " : "  ") << key << ": " << value << "\n";
        firstField = false;
    }
}

std::string createZeInfo(const std::vector<KernelSignature> &kernels, const BuiltIn::AddressingMode &mode, const HardwareInfo &hwInfo, bool heapless, uint32_t inlineDataSize) {
    constexpr uint32_t simdSize = 32;
    constexpr uint32_t pointerSize = sizeof(uint64_t);
    constexpr uint32_t bindlessImageOffsetSize = sizeof(uint64_t);
    constexpr uint32_t bindlessBufferOffsetSize = sizeof(uint32_t);
    constexpr std::string_view argIndent = "      ";
    const uint32_t localIdSize = alignUp(simdSize * static_cast<uint32_t>(sizeof(uint16_t)), hwInfo.capabilityTable.grfSize) * 3;
    const bool statelessBuffers = mode.bufferMode == BuiltIn::AddressingMode::BufferMode::stateless;
    const bool bindlessBuffers = mode.bufferMode == BuiltIn::AddressingMode::BufferMode::bindless;
    const bool bindlessImages = mode.imageMode == BuiltIn::AddressingMode::ImageMode::bindless;
    using std::to_string;

    std::ostringstream zeInfo;
    zeInfo << "---\nversion: '" << Zebin::ZeInfo::zeInfoDecoderVersion.major << "." << Zebin::ZeInfo::zeInfoDecoderVersion.minor << "'\n";
    zeInfo << "kernels:\n";
    for (const auto &kernel : kernels) {
        zeInfo << "  - name: " << kernel.name << "\n";
        zeInfo << "    execution_env:\n";
        zeInfo << "      grf_count: 128\n";
        zeInfo << "      simd_size: " << simdSize << "\n";
        if (statelessBuffers) {
            zeInfo << "      has_4gb_buffers: true\n";
        }
        if (inlineDataSize > 0) {
            zeInfo << "      inline_data_payload_size: " << inlineDataSize << "\n";
        }

        uint32_t offset = 0;
        auto addPayloadArg = [&](uint32_t size, uint32_t alignment) {
            offset = alignUp(offset, alignment);
            auto argOffset = offset;
            offset += size;
            return to_string(argOffset);
        };

        zeInfo << "    payload_arguments:\n";
        if (heapless) {
            writeListEntry(zeInfo, argIndent, {{"arg_type", "indirect_data_pointer"}, {"offset", addPayloadArg(pointerSize, pointerSize)}, {"size", to_string(pointerSize)}});
            writeListEntry(zeInfo, argIndent, {{"arg_type", "scratch_pointer"}, {"offset", addPayloadArg(pointerSize, pointerSize)}, {"size", to_string(pointerSize)}});
        }
        writeListEntry(zeInfo, argIndent, {{"arg_type", "global_id_offset"}, {"offset", addPayloadArg(12, 4)}, {"size", "12"}});
        writeListEntry(zeInfo, argIndent, {{"arg_type", "local_size"}, {"offset", addPayloadArg(12, 4)}, {"size", "12"}});
        writeListEntry(zeInfo, argIndent, {{"arg_type", "enqueued_local_size"}, {"offset", addPayloadArg(12, 4)}, {"size", "12"}});

        std::vector<uint32_t> bindfulArgs;
        for (uint32_t argIndex = 0; argIndex < kernel.args.size(); ++argIndex) {
            const auto &arg = kernel.args[argIndex];
            switch (arg.kind) {
            case KernelArg::Kind::value:
                writeListEntry(zeInfo, argIndent, {{"arg_type", "arg_byvalue"}, {"offset", addPayloadArg(arg.size, std::min(arg.size, 16u))}, {"size", to_string(arg.size)}, {"arg_index", to_string(argIndex)}});
                break;
            case KernelArg::Kind::globalPointer:
                if (statelessBuffers) {
                    writeListEntry(zeInfo, argIndent, {{"arg_type", "arg_bypointer"}, {"offset", addPayloadArg(pointerSize, pointerSize)}, {"size", to_string(pointerSize)}, {"arg_index", to_string(argIndex)}, {"addrmode", "stateless"}, {"addrspace", "global"}, {"access_type", getAccessType(arg)}});
                } else if (bindlessBuffers) {
                    writeListEntry(zeInfo, argIndent, {{"arg_type", "arg_bypointer"}, {"offset", addPayloadArg(bindlessBufferOffsetSize, bindlessBufferOffsetSize)}, {"size", to_string(bindlessBufferOffsetSize)}, {"arg_index", to_string(argIndex)}, {"addrmode", "bindless"}, {"addrspace", "global"}, {"access_type", getAccessType(arg)}});
                } else {
                    writeListEntry(zeInfo, argIndent, {{"arg_type", "arg_bypointer"}, {"offset", "0"}, {"size", "0"}, {"arg_index", to_string(argIndex)}, {"addrmode", "stateful"}, {"addrspace", "global"}, {"access_type", getAccessType(arg)}});
                    bindfulArgs.push_back(argIndex);
                }
                break;
            case KernelArg::Kind::image:
                if (bindlessImages) {
                    writeListEntry(zeInfo, argIndent, {{"arg_type", "arg_bypointer"}, {"offset", addPayloadArg(bindlessImageOffsetSize, bindlessImageOffsetSize)}, {"size", to_string(bindlessImageOffsetSize)}, {"arg_index", to_string(argIndex)}, {"addrmode", "bindless"}, {"addrspace", "image"}, {"access_type", getAccessType(arg)}, {"image_type", getImageType(arg.typeName)}});
                } else {
                    writeListEntry(zeInfo, argIndent, {{"arg_type", "arg_bypointer"}, {"offset", "0"}, {"size", "0"}, {"arg_index", to_string(argIndex)}, {"addrmode", "stateful"}, {"addrspace", "image"}, {"access_type", getAccessType(arg)}, {"image_type", getImageType(arg.typeName)}});
                    bindfulArgs.push_back(argIndex);
                }
                break;
            }
        }

        zeInfo << "    per_thread_payload_arguments:\n";
        writeListEntry(zeInfo, argIndent, {{"arg_type", "local_id"}, {"offset", "0"}, {"size", to_string(localIdSize)}});

        if (!bindfulArgs.empty()) {
            zeInfo << "    binding_table_indices:\n";
            for (uint32_t btiValue = 0; btiValue < bindfulArgs.size(); ++btiValue) {
                writeListEntry(zeInfo, argIndent, {{"bti_value", to_string(btiValue)}, {"arg_index", to_string(bindfulArgs[btiValue])}});
            }
        }
    }

    zeInfo << "kernels_misc_info:\n";
    for (const auto &kernel : kernels) {
        zeInfo << "  - name: " << kernel.name << "\n";
        if (kernel.args.empty()) {
            continue;
        }
        zeInfo << "    args_info:\n";
        for (uint32_t argIndex = 0; argIndex < kernel.args.size(); ++argIndex) {
            const auto &arg = kernel.args[argIndex];
            writeListEntry(zeInfo, argIndent, {{"index", to_string(argIndex)}, {"name", arg.name}, {"address_qualifier", arg.kind == KernelArg::Kind::value ? "__private" : "__global"}, {"access_qualifier", getAccessQualifier(arg)}, {"type_name", "'" + arg.typeName + ";" + to_string(arg.size) + "'"}, {"type_qualifiers", arg.isConst ? "const" : "NONE"}});
        }
    }
    return zeInfo.str();
}

std::vector<KernelSignature> parseKernelSignatures(std::string_view source, ConstStringRef typeDefinitions, std::string &outError) {
    const auto definitions = parseTypeDefinitions(typeDefinitions);
    const auto code = stripComments(source);
    constexpr std::string_view kernelKeyword = "__kernel";

    std::vector<KernelSignature> kernels;
    for (auto position = code.find(kernelKeyword); position != std::string::npos; position = code.find(kernelKeyword, position)) {
        position += kernelKeyword.size();
        auto openingParenthesis = code.find('(', position);
        auto closingParenthesis = code.find(')', openingParenthesis);
        if (openingParenthesis == std::string::npos || closingParenthesis == std::string::npos) {
            outError = "malformed kernel declaration";
            return {};
        }

        std::istringstream declaration(code.substr(position, openingParenthesis - position));
        std::string returnType;
        KernelSignature kernel;
        std::string unexpected;
        declaration >> returnType >> kernel.name;
        if (returnType != "void" || kernel.name.empty() || (declaration >> unexpected)) {
            outError = "unsupported kernel declaration: " + code.substr(position, openingParenthesis - position);
            return {};
        }

        std::istringstream argDeclarations(code.substr(openingParenthesis + 1, closingParenthesis - openingParenthesis - 1));
        std::string argDeclaration;
        while (std::getline(argDeclarations, argDeclaration, ',')) {
            auto arg = parseKernelArg(argDeclaration, definitions, outError);
            if (!arg) {
                outError = kernel.name + ": " + outError;
                return {};
            }
            kernel.args.push_back(*arg);
        }
        kernels.push_back(std::move(kernel));
        position = closingParenthesis;
    }
    return kernels;
}

std::vector<uint8_t> createZebin(const std::vector<KernelSignature> &kernels, const BuiltIn::AddressingMode &mode, const HardwareInfo &hwInfo) {
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto targetHwInfo = hwInfo;
    compilerProductHelper->adjustHwInfoForIgc(targetHwInfo);

    Elf::ElfEncoder<is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64> elfEncoder;
    auto &elfHeader = elfEncoder.getElfFileHeader();
    elfHeader.type = Zebin::Elf::ET_ZEBIN_EXE;
    elfHeader.machine = targetHwInfo.platform.eProductFamily;
    Zebin::Elf::ZebinTargetFlags flags{};
    flags.generatorId = static_cast<uint8_t>(GeneratorType::igc);
    elfHeader.flags = flags.packed;

    const std::array<uint8_t, 256> isa{};
    for (const auto &kernel : kernels) {
        elfEncoder.appendSection(Elf::SHT_PROGBITS, Zebin::Elf::SectionNames::textPrefix.str() + kernel.name, ArrayRef<const uint8_t>(isa.data(), isa.size()));
    }
    const bool heapless = compilerProductHelper->isHeaplessModeEnabled(hwInfo);
    const auto inlineDataSize = GfxCoreHelper::create(hwInfo.platform.eRenderCoreFamily)->getDefaultWalkerInlineDataSize();
    elfEncoder.appendSection(Zebin::Elf::SHT_ZEBIN_ZEINFO, Zebin::Elf::SectionNames::zeInfo, createZeInfo(kernels, mode, hwInfo, heapless, inlineDataSize));
    return elfEncoder.encode();
}

static_assert(packagedKernelTypes == CompilerOptions::kernelTypes && packagedWideStatelessTypes == CompilerOptions::wideStatelessTypes,
              "builtins are packaged with other type definitions than the runtime compiles them with");

bool isPackaged(const std::string &resourceName) {
    return std::ranges::find(packagedBuiltinResources, resourceName) != packagedBuiltinResources.end();
}

void registerResource(std::string name, std::vector<uint8_t> resource) {
    struct Storage {
        std::list<std::string> names;
        std::list<std::vector<uint8_t>> resources;
        std::list<RegisterEmbeddedResource> registrations;
    };
    // registered resources stay linked in the global registry, so they are never destroyed
    static auto &storage = *new Storage;

    if (RegisterEmbeddedResource::find(name) != nullptr) {
        return;
    }
    const auto &storedResource = storage.resources.emplace_back(std::move(resource));
    const auto &storedName = storage.names.emplace_back(std::move(name));
    storage.registrations.emplace_back(storedName.c_str(), reinterpret_cast<const char *>(storedResource.data()), storedResource.size());
}

} // namespace

void registerForDevice(const HardwareInfo &hwInfo) {
    for (uint32_t kernelIndex = 0; kernelIndex < BuiltIn::toIndex(BuiltIn::BaseKernel::count); ++kernelIndex) {
        const auto baseKernel = static_cast<BuiltIn::BaseKernel>(kernelIndex);

        auto source = RegisterEmbeddedResource::find(BuiltIn::getResourceNames(baseKernel, {}, BuiltIn::CodeType::source, hwInfo).back());
        if (source == nullptr) {
            continue;
        }
        const std::string_view sourceCode{source->resource};

        for (const auto &mode : BuiltIn::allAddressingModes) {
            auto resourceName = BuiltIn::getResourceNames(baseKernel, mode, BuiltIn::CodeType::binary, hwInfo)[0];
            if (!isPackaged(resourceName)) {
                continue;
            }
            std::string error;
            auto kernels = parseKernelSignatures(sourceCode, mode.wideMode ? packagedWideStatelessTypes : packagedKernelTypes, error);
            if (!error.empty() || kernels.empty()) {
                std::cerr << "Cannot create mock builtin binary for " << BuiltIn::getAsString(baseKernel) << ": " << (error.empty() ? "no kernels found" : error) << std::endl;
                UNRECOVERABLE_IF(true);
            }
            registerResource(std::move(resourceName), createZebin(kernels, mode, hwInfo));
        }
    }
}

void registerIntermediateRepresentations() {
    std::vector<uint8_t> intermediateRepresentation(64, 0);
    memcpy_s(intermediateRepresentation.data(), intermediateRepresentation.size(), spirvMagic.data(), spirvMagic.size());

    // the last name is the device independent one
    const HardwareInfo anyDevice{};
    for (uint32_t kernelIndex = 0; kernelIndex < BuiltIn::toIndex(BuiltIn::BaseKernel::count); ++kernelIndex) {
        for (const auto &mode : BuiltIn::allAddressingModes) {
            auto resourceName = BuiltIn::getResourceNames(static_cast<BuiltIn::BaseKernel>(kernelIndex), mode, BuiltIn::CodeType::intermediate, anyDevice).back();
            if (isPackaged(resourceName)) {
                registerResource(std::move(resourceName), intermediateRepresentation);
            }
        }
    }
}

} // namespace MockBuiltinBinaries
} // namespace NEO
