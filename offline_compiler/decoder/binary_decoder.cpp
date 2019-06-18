/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "binary_decoder.h"

#include "core/helpers/ptr_math.h"
#include "elf/reader.h"
#include "runtime/helpers/file_io.h"

#include "helper.h"

#include <cstring>
#include <fstream>

void BinaryDecoder::setMessagePrinter(const MessagePrinter &messagePrinter) {
    this->messagePrinter = messagePrinter;
}

template <typename T>
T readUnaligned(void *ptr) {
    T retVal = 0;
    uint8_t *tmp1 = reinterpret_cast<uint8_t *>(ptr);
    uint8_t *tmp2 = reinterpret_cast<uint8_t *>(&retVal);
    for (uint8_t i = 0; i < sizeof(T); ++i) {
        *(tmp2++) = *(tmp1++);
    }
    return retVal;
}

int BinaryDecoder::decode() {
    parseTokens();
    std::ofstream ptmFile(pathToDump + "PTM.txt");
    auto devBinPtr = getDevBinary();
    if (devBinPtr == nullptr) {
        messagePrinter.printf("Error! Device Binary section was not found.\n");
        exit(1);
    }
    return processBinary(devBinPtr, ptmFile);
}

void BinaryDecoder::dumpField(void *&binaryPtr, const PTField &field, std::ostream &ptmFile) {
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
        messagePrinter.printf("Error! Unknown size.\n");
        exit(1);
    }
    binaryPtr = ptrOffset(binaryPtr, field.size);
}

void *BinaryDecoder::getDevBinary() {
    binary = readBinaryFile(binaryFile);
    char *data = nullptr;
    CLElfLib::CElfReader elfReader(binary);
    for (const auto &sectionHeader : elfReader.getSectionHeaders()) { //Finding right section
        switch (sectionHeader.Type) {
        case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY: {
            std::ofstream ofs(pathToDump + "llvm.bin", std::ios::binary);
            ofs.write(elfReader.getSectionData(sectionHeader.DataOffset), sectionHeader.DataSize);
            break;
        }
        case CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV: {
            std::ofstream ofs(pathToDump + "spirv.bin", std::ios::binary);
            ofs.write(elfReader.getSectionData(sectionHeader.DataOffset), sectionHeader.DataSize);
            break;
        }
        case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS: {
            std::ofstream ofs(pathToDump + "build.bin", std::ios::binary);
            ofs.write(elfReader.getSectionData(sectionHeader.DataOffset), sectionHeader.DataSize);
            break;
        }
        case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY: {
            data = elfReader.getSectionData(sectionHeader.DataOffset);
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
        messagePrinter.printf("Unhandled type : %s\n", typeStr.c_str());
        exit(1);
    }
}

void BinaryDecoder::parseTokens() {
    //Creating patchlist definitions
    std::vector<std::string> patchList;

    if (pathToPatch.empty()) {
        messagePrinter.printf("Path to patch list not provided - using defaults, skipping patchokens as undefined.\n");
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

    size_t pos = findPos(patchList, "struct SProgramBinaryHeader");
    if (pos == patchList.size()) {
        messagePrinter.printf("While parsing patchtoken definitions: couldn't find SProgramBinaryHeader.");
        exit(1);
    }
    pos = findPos(patchList, "enum PATCH_TOKEN");
    if (pos == patchList.size()) {
        messagePrinter.printf("While parsing patchtoken definitions: couldn't find enum PATCH_TOKEN.");
        exit(1);
    }
    pos = findPos(patchList, "struct SKernelBinaryHeader");
    if (pos == patchList.size()) {
        messagePrinter.printf("While parsing patchtoken definitions: couldn't find SKernelBinaryHeader.");
        exit(1);
    }
    pos = findPos(patchList, "struct SKernelBinaryHeaderCommon :");
    if (pos == patchList.size()) {
        messagePrinter.printf("While parsing patchtoken definitions: couldn't find SKernelBinaryHeaderCommon.");
        exit(1);
    }

    // Reading all Patch Tokens and according structs
    uint8_t patchNo = 0;
    size_t patchTokenEnumPos = findPos(patchList, "enum PATCH_TOKEN");
    if (patchTokenEnumPos == patchList.size()) {
        exit(1);
    }

    for (auto i = patchTokenEnumPos + 1; i < patchList.size(); ++i) {
        if (patchList[i].find("};") != std::string::npos) {
            break;
        } else if (patchList[i].find("PATCH_TOKEN") == std::string::npos) {
            continue;
        } else if (patchList[i].find("@") == std::string::npos) {
            patchNo++;
            continue;
        }

        auto patchTokenPtr = std::make_unique<PatchToken>();
        size_t nameStartPos, nameEndPos;
        nameStartPos = patchList[i].find("PATCH_TOKEN");
        nameEndPos = patchList[i].find(',', nameStartPos);
        patchTokenPtr->name = patchList[i].substr(nameStartPos, nameEndPos - nameStartPos);

        nameStartPos = patchList[i].find("@");
        nameEndPos = patchList[i].find('@', nameStartPos + 1);
        if (nameEndPos == std::string::npos) {
            patchNo++;
            continue;
        }
        std::string structName = "struct " + patchList[i].substr(nameStartPos + 1, nameEndPos - nameStartPos - 1) + " :";

        size_t structPos = findPos(patchList, structName);
        if (structPos == patchList.size()) {
            patchNo++;
            continue;
        }
        patchTokenPtr->size = readStructFields(patchList, structPos + 1, patchTokenPtr->fields);
        patchTokens[patchNo++] = std::move(patchTokenPtr);
    }

    //Finding and reading Program Binary Header
    size_t structPos = findPos(patchList, "struct SProgramBinaryHeader") + 1;
    programHeader.size = readStructFields(patchList, structPos, programHeader.fields);

    //Finding and reading Kernel Binary Header
    structPos = findPos(patchList, "struct SKernelBinaryHeader") + 1;
    kernelHeader.size = readStructFields(patchList, structPos, kernelHeader.fields);

    structPos = findPos(patchList, "struct SKernelBinaryHeaderCommon :") + 1;
    kernelHeader.size += readStructFields(patchList, structPos, kernelHeader.fields);
}

void BinaryDecoder::printHelp() {
    messagePrinter.printf("Usage:\n-file <Opencl elf binary file> -patch <path to folder containing patchlist> -dump <path to dumping folder>\n");
    messagePrinter.printf("e.g. -file C:/my_folder/my_binary.bin -patch C:/igc/inc -dump C:/my_folder/dump\n");
}

int BinaryDecoder::processBinary(void *&ptr, std::ostream &ptmFile) {
    ptmFile << "ProgramBinaryHeader:\n";
    uint32_t numberOfKernels = 0, patchListSize = 0;
    for (const auto &v : programHeader.fields) {
        if (v.name == "NumberOfKernels") {
            numberOfKernels = readUnaligned<uint32_t>(ptr);
        } else if (v.name == "PatchListSize") {
            patchListSize = readUnaligned<uint32_t>(ptr);
        }
        dumpField(ptr, v, ptmFile);
    }
    if (numberOfKernels == 0) {
        messagePrinter.printf("Warning! Number of Kernels is 0.\n");
    }

    readPatchTokens(ptr, patchListSize, ptmFile);

    //Reading Kernels
    for (uint32_t i = 0; i < numberOfKernels; ++i) {
        ptmFile << "Kernel #" << i << '\n';
        processKernel(ptr, ptmFile);
    }
    return 0;
}

void BinaryDecoder::processKernel(void *&ptr, std::ostream &ptmFile) {
    uint32_t KernelNameSize = 0, KernelPatchListSize = 0, KernelHeapSize = 0,
             GeneralStateHeapSize = 0, DynamicStateHeapSize = 0, SurfaceStateHeapSize = 0;
    ptmFile << "KernelBinaryHeader:\n";
    for (const auto &v : kernelHeader.fields) {
        if (v.name == "PatchListSize")
            KernelPatchListSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "KernelNameSize")
            KernelNameSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "KernelHeapSize")
            KernelHeapSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "GeneralStateHeapSize")
            GeneralStateHeapSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "DynamicStateHeapSize")
            DynamicStateHeapSize = readUnaligned<uint32_t>(ptr);
        else if (v.name == "SurfaceStateHeapSize")
            SurfaceStateHeapSize = readUnaligned<uint32_t>(ptr);

        dumpField(ptr, v, ptmFile);
    }

    if (KernelNameSize == 0) {
        messagePrinter.printf("Error! KernelNameSize was 0.\n");
        exit(1);
    }

    ptmFile << "\tKernelName ";
    std::string kernelName(static_cast<const char *>(ptr), 0, KernelNameSize);
    ptmFile << kernelName << '\n';
    ptr = ptrOffset(ptr, KernelNameSize);

    std::string fileName = pathToDump + kernelName + "_KernelHeap.bin";
    writeDataToFile(fileName.c_str(), ptr, KernelHeapSize);
    ptr = ptrOffset(ptr, KernelHeapSize);

    if (GeneralStateHeapSize != 0) {
        messagePrinter.printf("Warning! GeneralStateHeapSize wasn't 0.\n");
        fileName = pathToDump + kernelName + "_GeneralStateHeap.bin";
        writeDataToFile(fileName.c_str(), ptr, DynamicStateHeapSize);
        ptr = ptrOffset(ptr, GeneralStateHeapSize);
    }

    fileName = pathToDump + kernelName + "_DynamicStateHeap.bin";
    writeDataToFile(fileName.c_str(), ptr, DynamicStateHeapSize);
    ptr = ptrOffset(ptr, DynamicStateHeapSize);

    fileName = pathToDump + kernelName + "_SurfaceStateHeap.bin";
    writeDataToFile(fileName.c_str(), ptr, SurfaceStateHeapSize);
    ptr = ptrOffset(ptr, SurfaceStateHeapSize);

    if (KernelPatchListSize == 0) {
        messagePrinter.printf("Warning! Kernel's patch list size was 0.\n");
    }
    readPatchTokens(ptr, KernelPatchListSize, ptmFile);
}

void BinaryDecoder::readPatchTokens(void *&patchListPtr, uint32_t patchListSize, std::ostream &ptmFile) {
    auto endPatchListPtr = ptrOffset(patchListPtr, patchListSize);
    while (patchListPtr != endPatchListPtr) {
        auto patchTokenPtr = patchListPtr;

        auto token = readUnaligned<uint32_t>(patchTokenPtr);
        patchTokenPtr = ptrOffset(patchTokenPtr, sizeof(uint32_t));

        auto Size = readUnaligned<uint32_t>(patchTokenPtr);
        patchTokenPtr = ptrOffset(patchTokenPtr, sizeof(uint32_t));

        if (patchTokens.count(token) > 0) {
            ptmFile << patchTokens[(token)]->name << ":\n";
        } else {
            ptmFile << "Unidentified PatchToken:\n";
        }

        ptmFile << '\t' << "4 Token " << token << '\n';
        ptmFile << '\t' << "4 Size " << Size << '\n';

        if (patchTokens.count(token) > 0) {
            uint32_t fieldsSize = 0;
            for (const auto &v : patchTokens[(token)]->fields) {
                if ((fieldsSize += v.size) > (Size - sizeof(uint32_t) * 2)) {
                    break;
                }
                if (v.name == "InlineDataSize") { // Because InlineData field value is not added to PT size
                    auto inlineDataSize = readUnaligned<uint32_t>(patchTokenPtr);
                    patchListPtr = ptrOffset(patchListPtr, inlineDataSize);
                }
                dumpField(patchTokenPtr, v, ptmFile);
            }
        }
        patchListPtr = ptrOffset(patchListPtr, Size);

        if (patchListPtr > patchTokenPtr) {
            ptmFile << "\tHex";
            uint8_t *byte = reinterpret_cast<uint8_t *>(patchTokenPtr);
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

int BinaryDecoder::validateInput(uint32_t argc, const char **argv) {
    if (!strcmp(argv[argc - 1], "--help")) {
        printHelp();
        return -1;
    } else {
        for (uint32_t i = 2; i < argc - 1; ++i) {
            if (!strcmp(argv[i], "-file")) {
                binaryFile = std::string(argv[++i]);
            } else if (!strcmp(argv[i], "-patch")) {
                pathToPatch = std::string(argv[++i]);
                addSlash(pathToPatch);
            } else if (!strcmp(argv[i], "-dump")) {
                pathToDump = std::string(argv[++i]);
                addSlash(pathToDump);
            } else {
                messagePrinter.printf("Unknown argument %s\n", argv[i]);
                printHelp();
                return -1;
            }
        }
        if (binaryFile.find(".bin") == std::string::npos) {
            messagePrinter.printf(".bin extension is expected for binary file.\n");
            printHelp();
            return -1;
        } else if (pathToDump.empty()) {
            messagePrinter.printf("Path to dump folder can't be empty.\n");
            printHelp();
            return -1;
        }
    }
    return 0;
}
