/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
    cl_int retVal = CL_SUCCESS;
    CLElfLib::CElfReader *pElfReader = nullptr;
    const CLElfLib::SElf64Header *pElfHeader = nullptr;
    char *pSectionData = nullptr;
    size_t sectionDataSize = 0;

    binaryVersion = iOpenCL::CURRENT_ICBE_VERSION;

    if (CLElfLib::CElfReader::isValidElf64(pBinary, binarySize) == false) {
        retVal = CL_INVALID_BINARY;
    }

    if (retVal == CL_SUCCESS) {
        elfBinarySize = binarySize;
        elfBinary = CLElfLib::ElfBinaryStorage(reinterpret_cast<const char *>(pBinary), reinterpret_cast<const char *>(reinterpret_cast<const char *>(pBinary) + binarySize));
    }

    if (retVal == CL_SUCCESS) {
        pElfReader = CLElfLib::CElfReader::create(
            (const char *)pBinary,
            binarySize);

        if (pElfReader == nullptr) {
            retVal = CL_OUT_OF_HOST_MEMORY;
        }
    }

    if (retVal == CL_SUCCESS) {
        pElfHeader = pElfReader->getElfHeader();

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
            retVal = CL_INVALID_BINARY;
        }
    }

    if (retVal == CL_SUCCESS) {
        // section 0 is always null
        for (uint32_t i = 1; i < pElfHeader->NumSectionHeaderEntries; i++) {
            const CLElfLib::SElf64SectionHeader *pSectionHeader = pElfReader->getSectionHeader(i);

            pSectionData = nullptr;
            sectionDataSize = 0;

            switch (pSectionHeader->Type) {
            case CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV:
                isSpirV = true;
                CPP_ATTRIBUTE_FALLTHROUGH;
            case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY:
                pElfReader->getSectionData(i, pSectionData, sectionDataSize);
                if (pSectionData && sectionDataSize) {
                    storeIrBinary(pSectionData, sectionDataSize, isSpirV);
                }
                break;

            case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY:
                pElfReader->getSectionData(i, pSectionData, sectionDataSize);
                if (pSectionData && sectionDataSize && validateGenBinaryHeader((SProgramBinaryHeader *)pSectionData)) {
                    storeGenBinary(pSectionData, sectionDataSize);
                    isCreatedFromBinary = true;
                } else {
                    getProgramCompilerVersion((SProgramBinaryHeader *)pSectionData, binaryVersion);
                    retVal = CL_INVALID_BINARY;
                }
                break;

            case CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS:
                pElfReader->getSectionData(i, pSectionData, sectionDataSize);
                if (pSectionData && sectionDataSize) {
                    options = pSectionData;
                }
                break;

            case CLElfLib::E_SH_TYPE::SH_TYPE_STR_TBL:
                // We can skip the string table
                break;

            default:
                retVal = CL_INVALID_BINARY;
            }

            if (retVal != CL_SUCCESS) {
                break;
            }
        }
    }

    if (retVal == CL_SUCCESS) {
        isProgramBinaryResolved = true;

        // Create an empty build log since program is effectively built
        updateBuildLog(pDevice, "", 1);
    }

    CLElfLib::CElfReader::destroy(pElfReader);
    return retVal;
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
