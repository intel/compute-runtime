/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

constexpr int minArgCount = 7;

static void showUsage(std::string name) {
    std::cerr << "Usage " << name << "<option(s)> - ALL BUT -p, --platform MUST BE SPECIFIED\n"
              << "Options :\n"
              << "\t -f, --file\t\tA file which content will be parsed into a uint32_t array in a .cpp file\n"
              << "\t -o, --output\t\t.Cpp output file name\n"
              << "\t -d, --device\t\tOPTIONAL - device ip version in format <major>_<minor>_<revision>\n"
              << "\t -a, --array\t\tName of an uin32_t type array containing parsed input file" << std::endl;
}
std::string parseToCharArray(std::unique_ptr<uint8_t[]> &binary, size_t size, std::string &builtinName, std::string &deviceIp, bool isSpirV) {
    std::ostringstream out;

    bool deviceIpPresent = !deviceIp.empty();
    out << "#include <cstddef>\n";
    out << "#include <cstdint>\n\n";
    out << "size_t " << builtinName;
    if (deviceIpPresent) {
        out << "BinarySize_" << deviceIp;
    } else {
        out << "BinarySize";
    }
    out << " = " << size << ";\n";
    out << "uint32_t " << builtinName;
    if (deviceIpPresent) {
        out << "Binary_" << deviceIp;
    } else {
        out << "Binary";
    }
    out << "[" << (size + 3) / 4 << "] = {"
        << std::endl
        << "    ";
    uint32_t *binaryUint = reinterpret_cast<uint32_t *>(binary.get());
    for (size_t i = 0; i < (size + 3) / 4; i++) {
        if (i != 0) {
            out << ", ";
            if (i % 8 == 0) {
                out << std::endl
                    << "    ";
            }
        }
        if (i < size / 4) {
            out << "0x" << std::hex << std::setw(8) << std::setfill('0') << binaryUint[i];
        } else {
            uint32_t lastBytes = size & 0x3;
            uint32_t lastUint = 0;
            uint8_t *pLastUint = (uint8_t *)&lastUint;
            for (uint32_t j = 0; j < lastBytes; j++) {
                pLastUint[sizeof(uint32_t) - 1 - j] = binary.get()[i * 4 + j];
            }
            out << "0x" << std::hex << std::setw(8) << std::setfill('0') << lastUint;
        }
    }
    out << "};" << std::endl;

    out << std::endl
        << "#include \"shared/source/built_ins/registry/built_ins_registry.h\"\n"
        << std::endl;
    out << "namespace NEO {" << std::endl;
    out << "static RegisterEmbeddedResource register" << builtinName;
    isSpirV ? out << "Ir(" : out << "Bin(";
    out << std::endl;
    out << "    \"";
    deviceIp != "" ? out << deviceIp << "_" << builtinName : out << builtinName;
    isSpirV ? out << ".builtin_kernel.spv\"," : out << ".builtin_kernel.bin\",";
    out << std::endl;
    out << "    (const char *)" << builtinName;
    if (deviceIpPresent) {
        out << "Binary_" << deviceIp;
    } else {
        out << "Binary";
    }
    out << "," << std::endl;
    out << "    " << builtinName;
    if (deviceIpPresent) {
        out << "BinarySize_" << deviceIp;
    } else {
        out << "BinarySize";
    }
    out << ");" << std::endl;
    out << "}" << std::endl;

    return out.str();
}

int main(int argc, char *argv[]) {
    if (argc < minArgCount) {
        showUsage(argv[0]);
        return 1;
    }
    std::string fileName;
    std::string cppOutputName;
    std::string arrayName;
    std::string deviceIp = "";
    size_t size = 0;
    std::fstream inputFile;
    bool isSpirV;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-f") || (arg == "--file")) {
            fileName = argv[++i];
        } else if ((arg == "-o") || (arg == "--output")) {
            cppOutputName = argv[++i];
        } else if ((arg == "-a") || (arg == "--array")) {
            arrayName = argv[++i];
        } else if ((arg == "-d") || (arg == "--device")) {
            deviceIp = argv[++i];
        } else {
            return 1;
        }
    }
    if (fileName.empty() || cppOutputName.empty() || arrayName.empty()) {
        std::cerr << "All three: fileName, cppOutputName and arrayName must be specified!" << std::endl;
        return 1;
    }
    inputFile.open(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    if (inputFile.is_open()) {
        size = static_cast<size_t>(inputFile.tellg());
        std::unique_ptr<uint8_t[]> memblock = std::make_unique<uint8_t[]>(size);
        inputFile.clear();
        inputFile.seekg(0, std::ios::beg);
        inputFile.read(reinterpret_cast<char *>(memblock.get()), size);
        inputFile.close();
        isSpirV = fileName.find(".spv") != std::string::npos;
        std::string cpp = parseToCharArray(memblock, size, arrayName, deviceIp, isSpirV);
        std::fstream(cppOutputName.c_str(), std::ios::out | std::ios::binary).write(cpp.c_str(), cpp.size());
    } else {
        std::cerr << "File cannot be opened!" << std::endl;
        return 1;
    }
    return 0;
}
