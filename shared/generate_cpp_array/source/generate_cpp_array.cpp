/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

constexpr int maxRetryCount = 3;
constexpr auto retryDelay = std::chrono::milliseconds(20);

static void showUsage(std::string name) {
    std::cerr << "Usage " << name << "<option(s)> - ALL BUT -d, --device MUST BE SPECIFIED\n"
              << "Options :\n"
              << "\t -l, --list\t\tA file with one input file per line, each input file content will be parsed into a uint32_t array named after the file in a single .cpp file\n"
              << "\t -o, --output\t\t.Cpp output file name\n"
              << "\t -d, --device\t\tOPTIONAL - device ip version in format <major>_<minor>_<revision>" << std::endl;
}

std::string parseToCharArray(std::unique_ptr<uint8_t[]> &binary, size_t size, const std::string &builtinName, const std::string &deviceIp, bool isSpirV) {
    std::ostringstream out;

    std::string suffix = deviceIp.empty() ? "" : "_" + deviceIp;
    std::string sizeName = builtinName + "BinarySize" + suffix;
    std::string arrayName = builtinName + "Binary" + suffix;

    out << "size_t " << sizeName << " = " << size << ";\n";
    out << "uint32_t " << arrayName << "[" << (size + 3) / 4 << "] = {"
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

    out << std::endl;
    out << "namespace NEO {" << std::endl;
    out << "static RegisterEmbeddedResource register" << builtinName;
    isSpirV ? out << "Ir" : out << "Bin";
    out << suffix << "(" << std::endl;
    out << "    \"";
    deviceIp != "" ? out << deviceIp << "_" << builtinName : out << builtinName;
    isSpirV ? out << ".builtin_kernel.spv\"," : out << ".builtin_kernel.bin\",";
    out << std::endl;
    out << "    (const char *)" << arrayName << "," << std::endl;
    out << "    " << sizeName << ");" << std::endl;
    out << "}" << std::endl;

    return out.str();
}

bool parseInputFile(const std::string &fileName, const std::string &deviceIp, std::ostringstream &out) {
    std::fstream inputFile;
    for (int retry = 0; retry < maxRetryCount; retry++) {
        inputFile.open(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
        if (inputFile.is_open()) {
            size_t size = static_cast<size_t>(inputFile.tellg());
            std::unique_ptr<uint8_t[]> memblock = std::make_unique<uint8_t[]>(size);
            inputFile.clear();
            inputFile.seekg(0, std::ios::beg);
            inputFile.read(reinterpret_cast<char *>(memblock.get()), size);
            inputFile.close();
            std::filesystem::path path(fileName);
            bool isSpirV = path.extension() == ".spv";
            out << std::endl
                << parseToCharArray(memblock, size, path.stem().string(), deviceIp, isSpirV);
            return true;
        }

        std::cerr << "Cannot open input file: " << fileName << std::endl;
        std::this_thread::sleep_for(retryDelay);
    }
    std::cerr << "Failed to open file after retrying." << fileName << std::endl;
    return false;
}

bool parseListFile(const std::string &listFileName, std::vector<std::string> &fileNames) {
    std::ifstream listFile(listFileName);
    if (!listFile.is_open()) {
        std::cerr << "Cannot open list file: " << listFileName << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(listFile, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            fileNames.push_back(line);
        }
    }
    return true;
}

int main(int argc, char *argv[]) {
    std::string listFileName;
    std::string cppOutputName;
    std::string deviceIp = "";
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (i + 1 == argc) {
            showUsage(argv[0]);
            return 1;
        }
        if ((arg == "-l") || (arg == "--list")) {
            listFileName = argv[++i];
        } else if ((arg == "-o") || (arg == "--output")) {
            cppOutputName = argv[++i];
        } else if ((arg == "-d") || (arg == "--device")) {
            deviceIp = argv[++i];
        } else {
            showUsage(argv[0]);
            return 1;
        }
    }
    if (listFileName.empty() || cppOutputName.empty()) {
        std::cerr << "Both: listFileName and cppOutputName must be specified!" << std::endl;
        showUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> fileNames;
    if (!parseListFile(listFileName, fileNames)) {
        return 1;
    }

    std::ostringstream out;
    out << "#include \"shared/source/built_ins/registry/built_ins_registry.h\"\n\n";
    out << "#include <cstddef>\n";
    out << "#include <cstdint>\n";
    for (const auto &fileName : fileNames) {
        if (!parseInputFile(fileName, deviceIp, out)) {
            return 1;
        }
    }

    std::string cpp = out.str();
    std::fstream(cppOutputName.c_str(), std::ios::out | std::ios::binary).write(cpp.c_str(), cpp.size());
    return 0;
}
