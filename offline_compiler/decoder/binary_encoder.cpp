/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "binary_encoder.h"

#include "elf/writer.h"
#include "runtime/helpers/file_io.h"

#include "CL/cl.h"
#include "helper.h"

#include <algorithm>
#include <cstring>
#include <fstream>

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
                printf("Warning! Calculated PatchListSize ( %u ) differs from file ( %u ) - changing it. Line %d\n", calcSize, size, static_cast<int>(patchListPos + 1));
                ptmFile[patchListPos] = ptmFile[patchListPos].substr(0, ptmFile[patchListPos].find_last_of(' ') + 1);
                ptmFile[patchListPos] += std::to_string(calcSize);
            }
        }
    }
}

int BinaryEncoder::copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary) {
    std::ifstream ifs(srcFileName, std::ios::binary);
    if (!ifs.good()) {
        printf("Cannot open %s.\n", srcFileName.c_str());
        return -1;
    }
    ifs.seekg(0, ifs.end);
    auto length = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0, ifs.beg);
    std::vector<char> binary(length);
    ifs.read(binary.data(), length);
    outBinary.write(binary.data(), length);
    return 0;
}

int BinaryEncoder::createElf() {
    CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

    //Build Options
    if (fileExists(pathToDump + "build.bin")) {
        auto binary = readBinaryFile(pathToDump + "build.bin");
        std::string data(binary.begin(), binary.end());
        elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS,
                                                    CLElfLib::E_SH_FLAG::SH_FLAG_NONE,
                                                    "BuildOptions",
                                                    data,
                                                    static_cast<uint32_t>(data.size())));
    } else {
        printf("Warning! Missing build section.\n");
    }

    //LLVM or SPIRV
    if (fileExists(pathToDump + "llvm.bin")) {
        auto binary = readBinaryFile(pathToDump + "llvm.bin");
        std::string data(binary.begin(), binary.end());
        elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY,
                                                    CLElfLib::E_SH_FLAG::SH_FLAG_NONE,
                                                    "Intel(R) OpenCL LLVM Object",
                                                    data,
                                                    static_cast<uint32_t>(data.size())));
    } else if (fileExists(pathToDump + "spirv.bin")) {
        auto binary = readBinaryFile(pathToDump + "spirv.bin");
        std::string data(binary.begin(), binary.end());
        elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV,
                                                    CLElfLib::E_SH_FLAG::SH_FLAG_NONE,
                                                    "SPIRV Object",
                                                    data,
                                                    static_cast<uint32_t>(data.size())));
    } else {
        printf("Warning! Missing llvm/spirv section.\n");
    }

    //Device Binary
    if (fileExists(pathToDump + "device_binary.bin")) {
        auto binary = readBinaryFile(pathToDump + "device_binary.bin");
        std::string data(binary.begin(), binary.end());
        elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY,
                                                    CLElfLib::E_SH_FLAG::SH_FLAG_NONE,
                                                    "Intel(R) OpenCL Device Binary",
                                                    data,
                                                    static_cast<uint32_t>(data.size())));
    } else {
        printf("Missing device_binary.bin\n");
        return -1;
    }

    //Resolve Elf Binary
    std::vector<char> elfBinary;
    elfWriter.resolveBinary(elfBinary);

    std::ofstream elfFile(elfName, std::ios::binary);
    if (!elfFile.good()) {
        printf("Couldn't create %s.\n", elfName.c_str());
        return -1;
    }

    elfFile.write(elfBinary.data(), elfBinary.size());
    return 0;
}

void BinaryEncoder::printHelp() {
    printf("Usage:\n-dump <path to dumping folder> -out <new elf file>\n");
    printf("e.g. -dump C:/my_folder/dump -out C:/my_folder/new_binary.bin\n");
}

int BinaryEncoder::encode() {
    std::vector<std::string> ptmFile;
    readFileToVectorOfStrings(ptmFile, pathToDump + "PTM.txt");
    calculatePatchListSizes(ptmFile);

    std::ofstream deviceBinary(pathToDump + "device_binary.bin", std::ios::binary);
    if (!deviceBinary.good()) {
        printf("Error! Couldn't create device_binary.bin.\n");
        return -1;
    }
    int retVal = processBinary(ptmFile, deviceBinary);
    deviceBinary.close();
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    return createElf();
}

int BinaryEncoder::processBinary(const std::vector<std::string> &ptmFile, std::ostream &deviceBinary) {
    size_t i = 0;
    while (i < ptmFile.size()) {
        if (ptmFile[i].find("Kernel #") != std::string::npos) {
            if (processKernel(++i, ptmFile, deviceBinary)) {
                printf("Error while processing kernel!\n");
                return -1;
            }
        } else if (writeDeviceBinary(ptmFile[i++], deviceBinary)) {
            printf("Error while writing to binary!\n");
            return -1;
        }
    }
    return 0;
}

int BinaryEncoder::processKernel(size_t &i, const std::vector<std::string> &ptmFile, std::ostream &deviceBinary) {
    uint32_t kernelNameSize = 0;
    while (i < ptmFile.size()) {
        if (ptmFile[i].find("KernelName ") != std::string::npos) {
            break;
        } else if (ptmFile[i].find("KernelNameSize") != std::string::npos) {
            std::stringstream ss(ptmFile[i]);
            ss.ignore(32, ' ');
            ss.ignore(32, ' ');
            ss >> kernelNameSize;
        }
        if (writeDeviceBinary(ptmFile[i++], deviceBinary)) {
            printf("Error while writing to binary.\n");
            return -1;
        }
    }
    //KernelName
    if (i == ptmFile.size()) {
        printf("Couldn't find KernelName line.\n");
        return -1;
    }
    std::string kernelName(ptmFile[i], ptmFile[i].find(' ') + 1);
    i++;

    deviceBinary.write(kernelName.c_str(), kernelName.size());
    for (auto j = kernelName.size(); j < kernelNameSize; ++j) {
        uint8_t nullByte = 0;
        deviceBinary.write(reinterpret_cast<const char *>(&nullByte), sizeof(uint8_t));
    }

    // Writing KernelHeap, DynamicStateHeap, SurfaceStateHeap
    if (fileExists(pathToDump + kernelName + "_GeneralStateHeap.bin")) {
        printf("Warning! Adding GeneralStateHeap.\n");
        if (copyBinaryToBinary(pathToDump + kernelName + "_GeneralStateHeap.bin", deviceBinary)) {
            printf("Error! Couldn't copy %s_GeneralStateHeap.bin\n", kernelName.c_str());
            return -1;
        }
    }
    if (copyBinaryToBinary(pathToDump + kernelName + "_KernelHeap.bin", deviceBinary)) {
        printf("Error! Couldn't copy %s_KernelHeap.bin\n", kernelName.c_str());
        return -1;
    }
    if (copyBinaryToBinary(pathToDump + kernelName + "_DynamicStateHeap.bin", deviceBinary)) {
        printf("Error! Couldn't copy %s_DynamicStateHeap.bin\n", kernelName.c_str());
        return -1;
    }
    if (copyBinaryToBinary(pathToDump + kernelName + "_SurfaceStateHeap.bin", deviceBinary)) {
        printf("Error! Couldn't copy %s_SurfaceStateHeap.bin\n", kernelName.c_str());
        return -1;
    }
    return 0;
}

int BinaryEncoder::validateInput(uint32_t argc, const char **argv) {
    if (!strcmp(argv[argc - 1], "--help")) {
        printHelp();
        return -1;
    } else {
        for (uint32_t i = 2; i < argc - 1; ++i) {
            if (!strcmp(argv[i], "-dump")) {
                pathToDump = std::string(argv[++i]);
                addSlash(pathToDump);
            } else if (!strcmp(argv[i], "-out")) {
                elfName = std::string(argv[++i]);
            } else {
                printf("Unknown argument %s\n", argv[i]);
                printHelp();
                return -1;
            }
        }
        if (pathToDump.empty()) {
            printf("Path to dump folder can't be empty.\n");
            printHelp();
            return -1;
        } else if (elfName.find(".bin") == std::string::npos) {
            printf(".bin extension is expected for binary file.\n");
            printHelp();
            return -1;
        }
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
            printf("Unknown size in line: %s\n", line.c_str());
            return -1;
        }
    }
    return 0;
}
