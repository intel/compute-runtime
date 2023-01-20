/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/binary_decoder.h"

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/utilities/directory.h"

#include <cstring>
#include <fstream>
#include <sstream>

template <typename T>
T readUnaligned(const void *ptr) {
    T retVal = 0;
    const uint8_t *tmp1 = reinterpret_cast<const uint8_t *>(ptr);
    uint8_t *tmp2 = reinterpret_cast<uint8_t *>(&retVal);
    for (uint8_t i = 0; i < sizeof(T); ++i) {
        *(tmp2++) = *(tmp1++);
    }
    return retVal;
}

int BinaryDecoder::decode() {
    parseTokens();
    std::stringstream ptmFile;
    auto devBinPtr = getDevBinary();
    if (devBinPtr == nullptr) {
        argHelper->printf("Error! Device Binary section was not found.\n");
        abortOclocExecution(1);
        return -1;
    }
    return processBinary(devBinPtr, ptmFile);
}

void BinaryDecoder::dumpField(const void *&binaryPtr, const PTField &field, std::ostream &ptmFile) {
    ptmFile << '\t' << static_cast<int>(field.size) << ' ';
    switch (field.size) {
    case 1: {
        auto val = readUnaligned<uint8_t>(binaryPtr);
        ptmFile << field.name << " " << +val << '\n';
        break;
    }
    case 2: {
        auto val = readUnaligned<uint16_t>(binaryPtr);
        ptmFile << field.name << " " << val << '\n';
        break;
    }
    case 4: {
        auto val = readUnaligned<uint32_t>(binaryPtr);
        ptmFile << field.name << " " << val << '\n';
        break;
    }
    case 8: {
        auto val = readUnaligned<uint64_t>(binaryPtr);
        ptmFile << field.name << " " << val << '\n';
        break;
    }
    default:
        argHelper->printf("Error! Unknown size.\n");
        abortOclocExecution(1);
    }
    binaryPtr = ptrOffset(binaryPtr, field.size);
}

template <typename ContainerT>
bool isPatchtokensBinary(const ContainerT &data) {
    static constexpr NEO::ConstStringRef intcMagic = "CTNI";
    auto binaryMagicLen = std::min(intcMagic.size(), data.size());
    NEO::ConstStringRef binaryMagic(reinterpret_cast<const char *>(&*data.begin()), binaryMagicLen);
    return intcMagic == binaryMagic;
}

const void *BinaryDecoder::getDevBinary() {
    binary = argHelper->readBinaryFile(binaryFile);
    const void *data = nullptr;
    if (isPatchtokensBinary(binary)) {
        return binary.data();
    }

    std::string decoderErrors;
    std::string decoderWarnings;
    auto input = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size());
    auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(input, decoderErrors, decoderWarnings);
    for (const auto &sectionHeader : elf.sectionHeaders) { // Finding right section
        auto sectionData = ArrayRef<const char>(reinterpret_cast<const char *>(sectionHeader.data.begin()), sectionHeader.data.size());
        switch (sectionHeader.header->type) {
        case NEO::Elf::SHT_OPENCL_LLVM_BINARY: {
            argHelper->saveOutput(pathToDump + "llvm.bin", sectionData.begin(), sectionData.size());
            break;
        }
        case NEO::Elf::SHT_OPENCL_SPIRV: {
            argHelper->saveOutput(pathToDump + "spirv.bin", sectionData.begin(), sectionData.size());
            break;
        }
        case NEO::Elf::SHT_OPENCL_OPTIONS: {
            argHelper->saveOutput(pathToDump + "build.bin", sectionData.begin(), sectionData.size());
            break;
        }
        case NEO::Elf::SHT_OPENCL_DEV_BINARY: {
            data = sectionData.begin();
            break;
        }
        default:
            break;
        }
    }
    return data;
}

uint8_t BinaryDecoder::getSize(const std::string &typeStr) {
    if (typeStr == "uint8_t") {
        return 1;
    } else if (typeStr == "uint16_t") {
        return 2;
    } else if (typeStr == "uint32_t") {
        return 4;
    } else if (typeStr == "uint64_t") {
        return 8;
    } else {
        argHelper->printf("Unhandled type : %s\n", typeStr.c_str());
        exit(1);
    }
}

std::vector<std::string> BinaryDecoder::loadPatchList() {
    if (argHelper->hasHeaders()) {
        return argHelper->headersToVectorOfStrings();
    } else {
        std::vector<std::string> patchList;
        if (pathToPatch.empty()) {
            argHelper->printf("Path to patch list not provided - using defaults, skipping patchtokens as undefined.\n");
            patchList = {
                "struct SProgramBinaryHeader",
                "{",
                "    uint32_t   Magic;",
                "    uint32_t   Version;",
                "    uint32_t   Device;",
                "    uint32_t   GPUPointerSizeInBytes;",
                "    uint32_t   NumberOfKernels;",
                "    uint32_t   SteppingId;",
                "    uint32_t   PatchListSize;",
                "};",
                "",
                "struct SKernelBinaryHeader",
                "{",
                "    uint32_t   CheckSum;",
                "    uint64_t   ShaderHashCode;",
                "    uint32_t   KernelNameSize;",
                "    uint32_t   PatchListSize;",
                "};",
                "",
                "struct SKernelBinaryHeaderCommon :",
                "       SKernelBinaryHeader",
                "{",
                "    uint32_t   KernelHeapSize;",
                "    uint32_t   GeneralStateHeapSize;",
                "    uint32_t   DynamicStateHeapSize;",
                "    uint32_t   SurfaceStateHeapSize;",
                "    uint32_t   KernelUnpaddedSize;",
                "};",
                "",
                "enum PATCH_TOKEN",
                "{",
                "    PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO,             // 41 @SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo@",
                "    PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO,           // 42 @SPatchAllocateConstantMemorySurfaceProgramBinaryInfo@",
                "};",
                "struct SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo :",
                "    SPatchItemHeader",
                "{",
                "    uint32_t   Type;",
                "    uint32_t   GlobalBufferIndex;",
                "    uint32_t   InlineDataSize;",
                "};",
                "struct SPatchAllocateConstantMemorySurfaceProgramBinaryInfo :",
                "    SPatchItemHeader",
                "{",
                "    uint32_t   ConstantBufferIndex;",
                "    uint32_t   InlineDataSize;",
                "};",

            };
        } else {
            readFileToVectorOfStrings(patchList, pathToPatch + "patch_list.h", true);
            readFileToVectorOfStrings(patchList, pathToPatch + "patch_shared.h", true);
            readFileToVectorOfStrings(patchList, pathToPatch + "patch_g7.h", true);
            readFileToVectorOfStrings(patchList, pathToPatch + "patch_g8.h", true);
            readFileToVectorOfStrings(patchList, pathToPatch + "patch_g9.h", true);
            readFileToVectorOfStrings(patchList, pathToPatch + "patch_g10.h", true);
        }
        return patchList;
    }
}

void BinaryDecoder::parseTokens() {
    // Creating patchlist definitions
    auto patchList = loadPatchList();

    size_t pos = findPos(patchList, "struct SProgramBinaryHeader");
    if (pos == patchList.size()) {
        argHelper->printf("While parsing patchtoken definitions: couldn't find SProgramBinaryHeader.");
        abortOclocExecution(1);
    }

    size_t patchTokenEnumPos = findPos(patchList, "enum PATCH_TOKEN");
    if (patchTokenEnumPos == patchList.size()) {
        argHelper->printf("While parsing patchtoken definitions: couldn't find enum PATCH_TOKEN.");
        abortOclocExecution(1);
    }

    pos = findPos(patchList, "struct SKernelBinaryHeader");
    if (pos == patchList.size()) {
        argHelper->printf("While parsing patchtoken definitions: couldn't find SKernelBinaryHeader.");
        abortOclocExecution(1);
    }

    pos = findPos(patchList, "struct SKernelBinaryHeaderCommon :");
    if (pos == patchList.size()) {
        argHelper->printf("While parsing patchtoken definitions: couldn't find SKernelBinaryHeaderCommon.");
        abortOclocExecution(1);
    }

    for (auto i = patchTokenEnumPos + 1; i < patchList.size(); ++i) {
        if (patchList[i].find("};") != std::string::npos) {
            break;
        } else if (patchList[i].find("PATCH_TOKEN") == std::string::npos) {
            continue;
        } else if (patchList[i].find("@") == std::string::npos) {
            continue;
        }

        size_t patchTokenNoStartPos, patchTokenNoEndPos;
        patchTokenNoStartPos = patchList[i].find('/') + 3;
        patchTokenNoEndPos = patchList[i].find(' ', patchTokenNoStartPos);
        std::stringstream patchTokenNoStream(patchList[i].substr(patchTokenNoStartPos, patchTokenNoEndPos - patchTokenNoStartPos));
        int patchNo;
        patchTokenNoStream >> patchNo;

        auto patchTokenPtr = std::make_unique<PatchToken>();
        size_t nameStartPos, nameEndPos;
        nameStartPos = patchList[i].find("PATCH_TOKEN");
        nameEndPos = patchList[i].find(',', nameStartPos);
        patchTokenPtr->name = patchList[i].substr(nameStartPos, nameEndPos - nameStartPos);

        nameStartPos = patchList[i].find("@");
        nameEndPos = patchList[i].find('@', nameStartPos + 1);
        if (nameEndPos == std::string::npos) {
            continue;
        }
        std::string structName = "struct " + patchList[i].substr(nameStartPos + 1, nameEndPos - nameStartPos - 1) + " :";

        size_t structPos = findPos(patchList, structName);
        if (structPos == patchList.size()) {
            continue;
        }
        patchTokenPtr->size = readStructFields(patchList, structPos + 1, patchTokenPtr->fields);
        patchTokens[static_cast<uint8_t>(patchNo)] = std::move(patchTokenPtr);
    }

    // Finding and reading Program Binary Header
    size_t structPos = findPos(patchList, "struct SProgramBinaryHeader") + 1;
    programHeader.size = readStructFields(patchList, structPos, programHeader.fields);

    // Finding and reading Kernel Binary Header
    structPos = findPos(patchList, "struct SKernelBinaryHeader") + 1;
    kernelHeader.size = readStructFields(patchList, structPos, kernelHeader.fields);

    structPos = findPos(patchList, "struct SKernelBinaryHeaderCommon :") + 1;
    kernelHeader.size += readStructFields(patchList, structPos, kernelHeader.fields);
}

void BinaryDecoder::printHelp() {
    argHelper->printf(R"===(Disassembles Intel Compute GPU device binary files.
Output of such operation is a set of files that can be later used to
reassemble back a valid Intel Compute GPU device binary (using ocloc 'asm'
command). This set of files contains:
Program-scope data :
  - spirv.bin (optional) - spirV representation of the program from which
                           the input binary was generated
  - build.bin            - build options that were used when generating the
                           input binary
  - PTM.txt              - 'patch tokens' describing program-scope and
                           kernel-scope metadata about the input binary

Kernel-scope data (<kname> is replaced by corresponding kernel's name):
  - <kname>_DynamicStateHeap.bin - initial DynamicStateHeap (binary file)
  - <kname>_SurfaceStateHeap.bin - initial SurfaceStateHeap (binary file)
  - <kname>_KernelHeap.asm       - list of instructions describing
                                   the kernel function (text file)

Usage: ocloc disasm -file <file> [-patch <patchtokens_dir>] [-dump <dump_dir>] [-device <device_type>] [-ignore_isa_padding]
  -file <file>              Input file to be disassembled.
                            This file should be an Intel Compute GPU device binary.

  -patch <patchtokens_dir>  Optional path to the directory containing
                            patchtoken definitions (patchlist.h, etc.)
                            as defined in intel-graphics-compiler (IGC) repo,
                            IGC subdirectory :
                            IGC/AdaptorOCL/ocl_igc_shared/executable_format
                            By default (when patchtokens_dir is not provided)
                            patchtokens won't be decoded.

  -dump <dump_dir>          Optional path for files representing decoded binary.
                            Default is './dump'.

  -device <device_type>     Optional target device of input binary
                            <device_type> can be: %s
                            By default ocloc will pick base device within
                            a generation - i.e. both skl and kbl will
                            fallback to skl. If specific product (e.g. kbl)
                            is needed, provide it as device_type.

  -ignore_isa_padding       Ignores Kernel Heap padding - Kernel Heap binary
                            will be saved without padding.

  --help                    Print this usage message.

Examples:
  Disassemble Intel Compute GPU device binary
    ocloc disasm -file source_file_Gen9core.bin
)===",
                      argHelper->createStringForArgs(argHelper->productConfigHelper->getDeviceAcronyms()).c_str());
}

int BinaryDecoder::processBinary(const void *&ptr, std::ostream &ptmFile) {
    ptmFile << "ProgramBinaryHeader:\n";
    uint32_t numberOfKernels = 0, patchListSize = 0, device = 0;
    for (const auto &v : programHeader.fields) {
        if (v.name == "NumberOfKernels") {
            numberOfKernels = readUnaligned<uint32_t>(ptr);
        } else if (v.name == "PatchListSize") {
            patchListSize = readUnaligned<uint32_t>(ptr);
        } else if (v.name == "Device") {
            device = readUnaligned<uint32_t>(ptr);
        }
        dumpField(ptr, v, ptmFile);
    }
    if (numberOfKernels == 0) {
        argHelper->printf("Warning! Number of Kernels is 0.\n");
    }

    readPatchTokens(ptr, patchListSize, ptmFile);
    iga->setGfxCore(static_cast<GFXCORE_FAMILY>(device));

    // Reading Kernels
    for (uint32_t i = 0; i < numberOfKernels; ++i) {
        ptmFile << "Kernel #" << i << '\n';
        processKernel(ptr, ptmFile);
    }

    argHelper->saveOutput(pathToDump + "PTM.txt", ptmFile);
    return 0;
}

void BinaryDecoder::processKernel(const void *&ptr, std::ostream &ptmFile) {
    uint32_t kernelNameSize = 0, kernelPatchListSize = 0, kernelHeapSize = 0, kernelHeapUnpaddedSize = 0,
             generalStateHeapSize = 0, dynamicStateHeapSize = 0, surfaceStateHeapSize = 0;
    ptmFile << "KernelBinaryHeader:\n";
    for (const auto &v : kernelHeader.fields) {
        if (v.name == "PatchListSize")
            kernelPatchListSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "KernelNameSize")
            kernelNameSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "KernelHeapSize")
            kernelHeapSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "KernelUnpaddedSize")
            kernelHeapUnpaddedSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "GeneralStateHeapSize")
            generalStateHeapSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "DynamicStateHeapSize")
            dynamicStateHeapSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "SurfaceStateHeapSize")
            surfaceStateHeapSize = readUnaligned<uint32_t>(ptr);

        dumpField(ptr, v, ptmFile);
    }

    if (kernelNameSize == 0) {
        argHelper->printf("Error! KernelNameSize was 0.\n");
        abortOclocExecution(1);
    }

    ptmFile << "\tKernelName ";
    std::string kernelName(static_cast<const char *>(ptr), 0, kernelNameSize);
    ptmFile << kernelName << '\n';
    ptr = ptrOffset(ptr, kernelNameSize);

    std::string fileName = pathToDump + kernelName + "_KernelHeap";
    argHelper->printf("Trying to disassemble %s.krn\n", kernelName.c_str());
    std::string disassembledKernel;
    if (iga->tryDisassembleGenISA(ptr, kernelHeapUnpaddedSize, disassembledKernel)) {
        argHelper->saveOutput(fileName + ".asm", disassembledKernel.data(), disassembledKernel.size());
    } else {
        if (ignoreIsaPadding) {
            argHelper->saveOutput(fileName + ".dat", ptr, kernelHeapUnpaddedSize);
        } else {
            argHelper->saveOutput(fileName + ".dat", ptr, kernelHeapSize);
        }
    }
    ptr = ptrOffset(ptr, kernelHeapSize);

    if (generalStateHeapSize != 0) {
        argHelper->printf("Warning! GeneralStateHeapSize wasn't 0.\n");
        fileName = pathToDump + kernelName + "_GeneralStateHeap.bin";
        argHelper->saveOutput(fileName, ptr, dynamicStateHeapSize);
        ptr = ptrOffset(ptr, generalStateHeapSize);
    }

    fileName = pathToDump + kernelName + "_DynamicStateHeap.bin";
    argHelper->saveOutput(fileName, ptr, dynamicStateHeapSize);
    ptr = ptrOffset(ptr, dynamicStateHeapSize);

    fileName = pathToDump + kernelName + "_SurfaceStateHeap.bin";
    argHelper->saveOutput(fileName, ptr, surfaceStateHeapSize);
    ptr = ptrOffset(ptr, surfaceStateHeapSize);

    if (kernelPatchListSize == 0) {
        argHelper->printf("Warning! Kernel's patch list size was 0.\n");
    }
    readPatchTokens(ptr, kernelPatchListSize, ptmFile);
}

void BinaryDecoder::readPatchTokens(const void *&patchListPtr, uint32_t patchListSize, std::ostream &ptmFile) {
    auto endPatchListPtr = ptrOffset(patchListPtr, patchListSize);
    while (patchListPtr != endPatchListPtr) {
        auto patchTokenPtr = patchListPtr;

        auto token = readUnaligned<uint32_t>(patchTokenPtr);
        patchTokenPtr = ptrOffset(patchTokenPtr, sizeof(uint32_t));

        auto size = readUnaligned<uint32_t>(patchTokenPtr);
        patchTokenPtr = ptrOffset(patchTokenPtr, sizeof(uint32_t));

        if (patchTokens.count(token) > 0) {
            ptmFile << patchTokens[(token)]->name << ":\n";
        } else {
            ptmFile << "Unidentified PatchToken:\n";
        }

        ptmFile << '\t' << "4 Token " << token << '\n';
        ptmFile << '\t' << "4 Size " << size << '\n';

        if (patchTokens.count(token) > 0) {
            uint32_t fieldsSize = 0;
            for (const auto &v : patchTokens[(token)]->fields) {
                if ((fieldsSize += static_cast<uint32_t>(v.size)) > (size - sizeof(uint32_t) * 2)) {
                    break;
                }
                if (v.name == "InlineDataSize") { // Because InlineData field value is not added to PT size
                    auto inlineDataSize = readUnaligned<uint32_t>(patchTokenPtr);
                    patchListPtr = ptrOffset(patchListPtr, inlineDataSize);
                }
                dumpField(patchTokenPtr, v, ptmFile);
            }
        }
        patchListPtr = ptrOffset(patchListPtr, size);

        if (patchListPtr > patchTokenPtr) {
            ptmFile << "\tHex";
            const uint8_t *byte = reinterpret_cast<const uint8_t *>(patchTokenPtr);
            while (ptrDiff(patchListPtr, patchTokenPtr) != 0) {
                ptmFile << ' ' << std::hex << +*(byte++);
                patchTokenPtr = ptrOffset(patchTokenPtr, sizeof(uint8_t));
            }
            ptmFile << std::dec << '\n';
        }
    }
}

uint32_t BinaryDecoder::readStructFields(const std::vector<std::string> &patchList,
                                         const size_t &structPos, std::vector<PTField> &fields) {
    std::string typeStr, fieldName;
    uint8_t size;
    uint32_t fullSize = 0;
    size_t f1, f2;

    for (auto i = structPos; i < patchList.size(); ++i) {
        if (patchList[i].find("};") != std::string::npos) {
            break;
        } else if (patchList[i].find("int") == std::string::npos) {
            continue;
        }

        f1 = patchList[i].find_first_not_of(' ');
        f2 = patchList[i].find(' ', f1 + 1);

        typeStr = patchList[i].substr(f1, f2 - f1);
        size = getSize(typeStr);

        f1 = patchList[i].find_first_not_of(' ', f2);
        f2 = patchList[i].find(';');
        fieldName = patchList[i].substr(f1, f2 - f1);
        fields.push_back(PTField{size, fieldName});
        fullSize += size;
    }
    return fullSize;
}

int BinaryDecoder::validateInput(const std::vector<std::string> &args) {
    for (size_t argIndex = 2; argIndex < args.size(); ++argIndex) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ("-file" == currArg && hasMoreArgs) {
            binaryFile = args[++argIndex];
        } else if ("-device" == currArg && hasMoreArgs) {
            setProductFamilyForIga(args[++argIndex], iga.get(), argHelper);
        } else if ("-patch" == currArg && hasMoreArgs) {
            pathToPatch = args[++argIndex];
            addSlash(pathToPatch);
        } else if ("-dump" == currArg && hasMoreArgs) {
            pathToDump = args[++argIndex];
            addSlash(pathToDump);
        } else if ("--help" == currArg) {
            showHelp = true;
            return 0;
        } else if ("-ignore_isa_padding" == currArg) {
            ignoreIsaPadding = true;
        } else if ("-q" == currArg) {
            argHelper->getPrinterRef() = MessagePrinter(true);
            iga->setMessagePrinter(argHelper->getPrinterRef());
        } else {
            argHelper->printf("Unknown argument %s\n", currArg.c_str());
            return -1;
        }
    }
    if (false == iga->isKnownPlatform()) {
        argHelper->printf("Warning : missing or invalid -device parameter - results may be inaccurate\n");
    }
    if (!argHelper->outputEnabled()) {
        if (pathToDump.empty()) {
            argHelper->printf("Warning : Path to dump folder not specificed - using ./dump as default.\n");
            pathToDump = std::string("dump/");
        }
        NEO::Directory::createDirectory(pathToDump);
    }
    return 0;
}
