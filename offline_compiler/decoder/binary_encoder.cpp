/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "binary_encoder.h"

#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hash.h"
#include "offline_compiler/offline_compiler.h"

#include "CL/cl.h"
#include "helper.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

void BinaryEncoder::setMessagePrinter(const MessagePrinter &messagePrinter) {
    this->messagePrinter = messagePrinter;
}

void BinaryEncoder::calculatePatchListSizes(std::vector<std::string> &ptmFile) {
    size_t patchListPos = 0;
    for (size_t i = 0; i < ptmFile.size(); ++i) {
        if (ptmFile[i].find("PatchListSize") != std::string::npos) {
            patchListPos = i;
        } else if (ptmFile[i].find("PATCH_TOKEN") != std::string::npos) {
            uint32_t calcSize = 0;
            i++;
            while (i < ptmFile.size() && ptmFile[i].find("Kernel #") == std::string::npos) {
                if (ptmFile[i].find(':') == std::string::npos) {
                    if (ptmFile[i].find("Hex") != std::string::npos) {
                        calcSize += static_cast<uint32_t>(std::count(ptmFile[i].begin(), ptmFile[i].end(), ' '));
                    } else {
                        calcSize += std::atoi(&ptmFile[i][1]);
                    }
                }
                i++;
            }
            uint32_t size = static_cast<uint32_t>(std::stoul(ptmFile[patchListPos].substr(ptmFile[patchListPos].find_last_of(' ') + 1)));
            if (size != calcSize) {
                messagePrinter.printf("Warning! Calculated PatchListSize ( %u ) differs from file ( %u ) - changing it. Line %d\n", calcSize, size, static_cast<int>(patchListPos + 1));
                ptmFile[patchListPos] = ptmFile[patchListPos].substr(0, ptmFile[patchListPos].find_last_of(' ') + 1);
                ptmFile[patchListPos] += std::to_string(calcSize);
            }
        }
    }
}

bool BinaryEncoder::copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary, uint32_t *binaryLength) {
    std::ifstream ifs(srcFileName, std::ios::binary);
    if (!ifs.good()) {
        messagePrinter.printf("Cannot open %s.\n", srcFileName.c_str());
        return false;
    }
    ifs.seekg(0, ifs.end);
    auto length = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0, ifs.beg);
    std::vector<char> binary(length);
    ifs.read(binary.data(), length);
    outBinary.write(binary.data(), length);

    if (binaryLength) {
        *binaryLength = static_cast<uint32_t>(length);
    }

    return true;
}

int BinaryEncoder::createElf() {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> ElfEncoder;
    ElfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;

    //Build Options
    if (fileExists(pathToDump + "build.bin")) {
        auto binary = readBinaryFile(pathToDump + "build.bin");
        ElfEncoder.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, "BuildOptions",
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));
    } else {
        messagePrinter.printf("Warning! Missing build section.\n");
    }

    //LLVM or SPIRV
    if (fileExists(pathToDump + "llvm.bin")) {
        auto binary = readBinaryFile(pathToDump + "llvm.bin");
        ElfEncoder.appendSection(NEO::Elf::SHT_OPENCL_LLVM_BINARY, "Intel(R) OpenCL LLVM Object",
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));
    } else if (fileExists(pathToDump + "spirv.bin")) {
        auto binary = readBinaryFile(pathToDump + "spirv.bin");
        std::string data(binary.begin(), binary.end());
        ElfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, "SPIRV Object",
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));
    } else {
        messagePrinter.printf("Warning! Missing llvm/spirv section.\n");
    }

    //Device Binary
    if (fileExists(pathToDump + "device_binary.bin")) {
        auto binary = readBinaryFile(pathToDump + "device_binary.bin");
        ElfEncoder.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, "Intel(R) OpenCL Device Binary",
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));
    } else {
        messagePrinter.printf("Missing device_binary.bin\n");
        return -1;
    }

    //Resolve Elf Binary
    auto elfBinary = ElfEncoder.encode();

    std::ofstream elfFile(elfName, std::ios::binary);
    if (!elfFile.good()) {
        messagePrinter.printf("Couldn't create %s.\n", elfName.c_str());
        return -1;
    }

    elfFile.write(reinterpret_cast<const char *>(elfBinary.data()), elfBinary.size());
    return 0;
}

void BinaryEncoder::printHelp() {
    messagePrinter.printf(R"===(Assembles Intel OpenCL GPU device binary from input files.
It's expected that input files were previously generated by 'ocloc disasm'
command or are compatible with 'ocloc disasm' output (especially in terms of
file naming scheme). See 'ocloc disasm --help' for additional info.

Usage: ocloc asm -out <out_file> [-dump <dump_dir>] [-device <device_type>] [-ignore_isa_padding]
  -out <out_file>           Filename for newly assembled binary.

  -dump <dumping_dir>       Path to the input directory containing 
                            disassembled binary (as disassembled 
                            by ocloc's disasm command).
                            Default is './dump'.   

  -device <device_type>     Optional target device of output binary
                            <device_type> can be: %s
                            By default ocloc will pick base device within
                            a generation - i.e. both skl and kbl will
                            fallback to skl. If specific product (e.g. kbl)
                            is needed, provide it as device_type.

  -ignore_isa_padding       Ignores Kernel Heap padding - padding will not
                            be added to Kernel Heap binary.

  --help                    Print this usage message.

Examples:
  Assemble to Intel OpenCL GPU device binary
    ocloc asm -out reassembled.bin
)===",
                          NEO::getDevicesTypes().c_str());
}

int BinaryEncoder::encode() {
    std::vector<std::string> ptmFile;
    readFileToVectorOfStrings(ptmFile, pathToDump + "PTM.txt");
    calculatePatchListSizes(ptmFile);

    std::ofstream deviceBinary(pathToDump + "device_binary.bin", std::ios::binary);
    if (!deviceBinary.good()) {
        messagePrinter.printf("Error! Couldn't create device_binary.bin.\n");
        return -1;
    }
    int retVal = processBinary(ptmFile, deviceBinary);
    deviceBinary.close();
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    return createElf();
}

int BinaryEncoder::processBinary(const std::vector<std::string> &ptmFileLines, std::ostream &deviceBinary) {
    if (false == iga->isKnownPlatform()) {
        auto deviceMarker = findPos(ptmFileLines, "Device");
        if (deviceMarker != ptmFileLines.size()) {
            std::stringstream ss(ptmFileLines[deviceMarker]);
            ss.ignore(32, ' ');
            ss.ignore(32, ' ');
            uint32_t gfxCore = 0;
            ss >> gfxCore;
            iga->setGfxCore(static_cast<GFXCORE_FAMILY>(gfxCore));
        }
    }
    size_t i = 0;
    while (i < ptmFileLines.size()) {
        if (ptmFileLines[i].find("Kernel #") != std::string::npos) {
            if (processKernel(++i, ptmFileLines, deviceBinary)) {
                messagePrinter.printf("Warning while processing kernel!\n");
                return -1;
            }
        } else if (writeDeviceBinary(ptmFileLines[i++], deviceBinary)) {
            messagePrinter.printf("Error while writing to binary!\n");
            return -1;
        }
    }
    return 0;
}

void BinaryEncoder::addPadding(std::ostream &out, size_t numBytes) {
    for (size_t i = 0; i < numBytes; ++i) {
        const char nullByte = 0;
        out.write(&nullByte, 1U);
    }
}

int BinaryEncoder::processKernel(size_t &line, const std::vector<std::string> &ptmFileLines, std::ostream &deviceBinary) {
    auto kernelInfoBeginMarker = line;
    auto kernelInfoEndMarker = ptmFileLines.size();
    auto kernelNameMarker = ptmFileLines.size();
    auto kernelPatchtokensMarker = ptmFileLines.size();
    std::stringstream kernelBlob;

    // Normally these are added by the compiler, need to take or of them when reassembling
    constexpr size_t isaPaddingSizeInBytes = 128;
    constexpr uint32_t kernelHeapAlignmentInBytes = 64;

    uint32_t kernelNameSizeInBinary = 0;
    std::string kernelName;

    //  Scan PTM lines for kernel info
    while (line < ptmFileLines.size()) {
        if (ptmFileLines[line].find("KernelName ") != std::string::npos) {
            kernelName = std::string(ptmFileLines[line], ptmFileLines[line].find(' ') + 1);
            kernelNameMarker = line;
            kernelPatchtokensMarker = kernelNameMarker + 1; // patchtokens come after name
        } else if (ptmFileLines[line].find("KernelNameSize") != std::string::npos) {
            std::stringstream ss(ptmFileLines[line]);
            ss.ignore(32, ' ');
            ss.ignore(32, ' ');
            ss >> kernelNameSizeInBinary;
        } else if (ptmFileLines[line].find("Kernel #") != std::string::npos) {
            kernelInfoEndMarker = line;
            break;
        }
        ++line;
    }

    // Write KernelName and padding
    kernelBlob.write(kernelName.c_str(), kernelName.size());
    addPadding(kernelBlob, kernelNameSizeInBinary - kernelName.size());

    // Write KernelHeap and padding
    uint32_t kernelHeapSizeUnpadded = 0U;
    bool heapsCopiedSuccesfully = true;

    // Use .asm if available, fallback to .dat
    if (fileExists(pathToDump + kernelName + "_KernelHeap.asm")) {
        auto kernelAsAsm = readBinaryFile(pathToDump + kernelName + "_KernelHeap.asm");
        std::string kernelAsBinary;
        messagePrinter.printf("Trying to assemble %s.asm\n", kernelName.c_str());
        if (false == iga->tryAssembleGenISA(std::string(kernelAsAsm.begin(), kernelAsAsm.end()), kernelAsBinary)) {
            messagePrinter.printf("Error : Could not assemble : %s\n", kernelName.c_str());
            return -1;
        }
        kernelHeapSizeUnpadded = static_cast<uint32_t>(kernelAsBinary.size());
        kernelBlob.write(kernelAsBinary.data(), kernelAsBinary.size());
    } else {
        heapsCopiedSuccesfully = copyBinaryToBinary(pathToDump + kernelName + "_KernelHeap.dat", kernelBlob, &kernelHeapSizeUnpadded);
    }

    uint32_t kernelHeapSize = 0U;
    // Adding padding and alignment
    if (ignoreIsaPadding) {
        kernelHeapSize = kernelHeapSizeUnpadded;
    } else {
        addPadding(kernelBlob, isaPaddingSizeInBytes);
        const uint32_t kernelHeapPaddedSize = kernelHeapSizeUnpadded + isaPaddingSizeInBytes;
        kernelHeapSize = alignUp(kernelHeapPaddedSize, kernelHeapAlignmentInBytes);
        addPadding(kernelBlob, kernelHeapSize - kernelHeapPaddedSize);
    }

    // Write GeneralStateHeap, DynamicStateHeap, SurfaceStateHeap
    if (fileExists(pathToDump + kernelName + "_GeneralStateHeap.bin")) {
        heapsCopiedSuccesfully = heapsCopiedSuccesfully && copyBinaryToBinary(pathToDump + kernelName + "_GeneralStateHeap.bin", kernelBlob);
    }
    heapsCopiedSuccesfully = heapsCopiedSuccesfully && copyBinaryToBinary(pathToDump + kernelName + "_DynamicStateHeap.bin", kernelBlob);
    heapsCopiedSuccesfully = heapsCopiedSuccesfully && copyBinaryToBinary(pathToDump + kernelName + "_SurfaceStateHeap.bin", kernelBlob);
    if (false == heapsCopiedSuccesfully) {
        return -1;
    }

    // Write kernel patchtokens
    for (size_t i = kernelPatchtokensMarker; i < kernelInfoEndMarker; ++i) {
        if (writeDeviceBinary(ptmFileLines[i], kernelBlob)) {
            messagePrinter.printf("Error while writing to binary.\n");
            return -1;
        }
    }

    auto kernelBlobData = kernelBlob.str();
    uint64_t hashValue = NEO::Hash::hash(reinterpret_cast<const char *>(kernelBlobData.data()), kernelBlobData.size());
    uint32_t calcCheckSum = hashValue & 0xFFFFFFFF;

    // Add kernel header
    for (size_t i = kernelInfoBeginMarker; i < kernelNameMarker; ++i) {
        if (ptmFileLines[i].find("CheckSum") != std::string::npos) {
            static_assert(std::is_same<decltype(calcCheckSum), uint32_t>::value, "");
            deviceBinary.write(reinterpret_cast<char *>(&calcCheckSum), sizeof(uint32_t));
        } else if (ptmFileLines[i].find("KernelHeapSize") != std::string::npos) {
            static_assert(sizeof(kernelHeapSize) == sizeof(uint32_t), "");
            deviceBinary.write(reinterpret_cast<const char *>(&kernelHeapSize), sizeof(uint32_t));
        } else if (ptmFileLines[i].find("KernelUnpaddedSize") != std::string::npos) {
            static_assert(sizeof(kernelHeapSizeUnpadded) == sizeof(uint32_t), "");
            deviceBinary.write(reinterpret_cast<char *>(&kernelHeapSizeUnpadded), sizeof(uint32_t));
        } else {
            if (writeDeviceBinary(ptmFileLines[i], deviceBinary)) {
                messagePrinter.printf("Error while writing to binary.\n");
                return -1;
            }
        }
    }

    // Add kernel blob after the header
    deviceBinary.write(kernelBlobData.c_str(), kernelBlobData.size());

    return 0;
}

int BinaryEncoder::validateInput(uint32_t argc, const char **argv) {
    if (!strcmp(argv[argc - 1], "--help")) {
        printHelp();
        return -1;
    }

    for (uint32_t i = 2; i < argc; ++i) {
        if (i < argc - 1) {
            if (!strcmp(argv[i], "-dump")) {
                pathToDump = std::string(argv[++i]);
                addSlash(pathToDump);
            } else if (!strcmp(argv[i], "-device")) {
                iga->setProductFamily(getProductFamilyFromDeviceName(argv[++i]));
            } else if (!strcmp(argv[i], "-out")) {
                elfName = std::string(argv[++i]);
            } else {
                messagePrinter.printf("Unknown argument %s\n", argv[i]);
                printHelp();
                return -1;
            }
        } else {
            if (!strcmp(argv[i], "-ignore_isa_padding")) {
                ignoreIsaPadding = true;
            } else {
                messagePrinter.printf("Unknown argument %s\n", argv[i]);
                printHelp();
                return -1;
            }
        }
    }
    if (pathToDump.empty()) {
        messagePrinter.printf("Warning : Path to dump folder not specificed - using ./dump as default.\n");
        pathToDump = "dump";
        addSlash(pathToDump);
    }
    if (elfName.find(".bin") == std::string::npos) {
        messagePrinter.printf(".bin extension is expected for binary file.\n");
        printHelp();
        return -1;
    }

    if (false == iga->isKnownPlatform()) {
        messagePrinter.printf("Warning : missing or invalid -device parameter - results may be inacurate\n");
    }
    return 0;
}

template <typename T>
void BinaryEncoder::write(std::stringstream &in, std::ostream &deviceBinary) {
    T val;
    in >> val;
    deviceBinary.write(reinterpret_cast<const char *>(&val), sizeof(T));
}
template <>
void BinaryEncoder::write<uint8_t>(std::stringstream &in, std::ostream &deviceBinary) {
    uint8_t val;
    uint16_t help;
    in >> help;
    val = static_cast<uint8_t>(help);
    deviceBinary.write(reinterpret_cast<const char *>(&val), sizeof(uint8_t));
}
template void BinaryEncoder::write<uint16_t>(std::stringstream &in, std::ostream &deviceBinary);
template void BinaryEncoder::write<uint32_t>(std::stringstream &in, std::ostream &deviceBinary);
template void BinaryEncoder::write<uint64_t>(std::stringstream &in, std::ostream &deviceBinary);

int BinaryEncoder::writeDeviceBinary(const std::string &line, std::ostream &deviceBinary) {
    if (line.find(':') != std::string::npos) {
        return 0;
    } else if (line.find("Hex") != std::string::npos) {
        std::stringstream ss(line);
        ss.ignore(32, ' ');
        uint16_t tmp;
        uint8_t byte;
        while (!ss.eof()) {
            ss >> std::hex >> tmp;
            byte = static_cast<uint8_t>(tmp);
            deviceBinary.write(reinterpret_cast<const char *>(&byte), sizeof(uint8_t));
        }
    } else {
        std::stringstream ss(line);
        uint16_t size;
        std::string name;
        ss >> size;
        ss >> name;
        switch (size) {
        case 1:
            write<uint8_t>(ss, deviceBinary);
            break;
        case 2:
            write<uint16_t>(ss, deviceBinary);
            break;
        case 4:
            write<uint32_t>(ss, deviceBinary);
            break;
        case 8:
            write<uint64_t>(ss, deviceBinary);
            break;
        default:
            messagePrinter.printf("Unknown size in line: %s\n", line.c_str());
            return -1;
        }
    }
    return 0;
}

bool BinaryEncoder::fileExists(const std::string &path) const {
    return ::fileExists(path);
}

std::vector<char> BinaryEncoder::readBinaryFile(const std::string &path) const {
    return ::readBinaryFile(path);
}
