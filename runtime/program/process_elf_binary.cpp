/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "common/compiler_support.h"
#include "elf/reader.h"
#include "elf/writer.h"
#include "program.h"
#include "runtime/helpers/string.h"

namespace OCLRT {

cl_int Program::processElfBinary(
    const void *pBinary,
    size_t binarySize,
    uint32_t &binaryVersion) {
    const CLElfLib::SElf64Header *pElfHeader = nullptr;

    binaryVersion = iOpenCL::CURRENT_ICBE_VERSION;

    elfBinarySize = binarySize;
    elfBinary = CLElfLib::ElfBinaryStorage(reinterpret_cast<const char *>(pBinary), reinterpret_cast<const char *>(reinterpret_cast<const char *>(pBinary) + binarySize));

    try {
        CLElfLib::CElfReader elfReader(elfBinary);

        pElfHeader = elfReader.getElfHeader();

        switch (pElfHeader->Type) {
        case CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE:
            programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
            break;

        case CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY:
            programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
            break;

        case CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_OBJECTS:
            programBinaryType = CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
            break;

        default:
            return CL_INVALID_BINARY;
        }
        // section 0 is always null
        for (size_t i = 1u; i < elfReader.getSectionHeaders().size(); ++i) {
            const auto &sectionHeader = elfReader.getSectionHeaders()[i];
            switch (sectionHeader.Type) {
            case CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV:
                isSpirV = true;
                CPP_ATTRIBUTE_FALLTHROUGH;
            case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY:
                if (sectionHeader.DataSize > 0) {
                    storeIrBinary(elfReader.getSectionData(sectionHeader.DataOffset), static_cast<size_t>(sectionHeader.DataSize), isSpirV);
                }
                break;

            case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY:
                if (sectionHeader.DataSize > 0 && validateGenBinaryHeader(reinterpret_cast<SProgramBinaryHeader *>(elfReader.getSectionData(sectionHeader.DataOffset)))) {
                    storeGenBinary(elfReader.getSectionData(sectionHeader.DataOffset), static_cast<size_t>(sectionHeader.DataSize));
                    isCreatedFromBinary = true;
                } else {
                    getProgramCompilerVersion(reinterpret_cast<SProgramBinaryHeader *>(elfReader.getSectionData(sectionHeader.DataOffset)), binaryVersion);
                    return CL_INVALID_BINARY;
                }
                break;

            case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS:
                if (sectionHeader.DataSize > 0) {
                    options = std::string(elfReader.getSectionData(sectionHeader.DataOffset), static_cast<size_t>(sectionHeader.DataSize));
                }
                break;

            case CLElfLib::E_SH_TYPE::SH_TYPE_STR_TBL:
                // We can skip the string table
                break;

            default:
                return CL_INVALID_BINARY;
            }
        }

        isProgramBinaryResolved = true;

        // Create an empty build log since program is effectively built
        updateBuildLog(pDevice, "", 1);
    } catch (const CLElfLib::ElfException &) {
        return CL_INVALID_BINARY;
    }

    return CL_SUCCESS;
}

cl_int Program::resolveProgramBinary() {
    CLElfLib::E_EH_TYPE headerType;

    if (isProgramBinaryResolved == false) {
        elfBinary.clear();
        elfBinarySize = 0;

        switch (programBinaryType) {
        case CL_PROGRAM_BINARY_TYPE_EXECUTABLE:
            headerType = CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE;

            if (!genBinary || !genBinarySize) {
                return CL_INVALID_BINARY;
            }
            break;

        case CL_PROGRAM_BINARY_TYPE_LIBRARY:
            headerType = CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY;

            if (!irBinary || !irBinarySize) {
                return CL_INVALID_BINARY;
            }
            break;

        case CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT:
            headerType = CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_OBJECTS;

            if (!irBinary || !irBinarySize) {
                return CL_INVALID_BINARY;
            }
            break;

        default:
            return CL_INVALID_BINARY;
        }

        CLElfLib::CElfWriter elfWriter(headerType, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

        elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "BuildOptions", options, static_cast<uint32_t>(strlen(options.c_str()) + 1u)));
        std::string irBinaryTemp = irBinary ? std::string(irBinary, irBinarySize) : "";
        // Add the LLVM component if available
        elfWriter.addSection(CLElfLib::SSectionNode(getIsSpirV() ? CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV : CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY, CLElfLib::E_SH_FLAG::SH_FLAG_NONE,
                                                    headerType == CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY ? "Intel(R) OpenCL LLVM Archive" : "Intel(R) OpenCL LLVM Object", std::move(irBinaryTemp), static_cast<uint32_t>(irBinarySize)));
        // Add the device binary if it exists
        if (genBinary) {
            std::string genBinaryTemp = genBinary ? std::string(genBinary, genBinarySize) : "";
            elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "Intel(R) OpenCL Device Binary", std::move(genBinaryTemp), static_cast<uint32_t>(genBinarySize)));
        }

        // Add the device debug data if it exists
        if (debugData != nullptr) {
            std::string debugDataTemp = debugData ? std::string(debugData, debugDataSize) : "";
            elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_DEBUG, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "Intel(R) OpenCL Device Debug", std::move(debugDataTemp), static_cast<uint32_t>(debugDataSize)));
        }

        elfBinarySize = elfWriter.getTotalBinarySize();
        elfBinary = CLElfLib::ElfBinaryStorage(elfBinarySize);
        elfWriter.resolveBinary(elfBinary);
        isProgramBinaryResolved = true;
    } else {
        return CL_OUT_OF_HOST_MEMORY;
    }
    return CL_SUCCESS;
}
} // namespace OCLRT
