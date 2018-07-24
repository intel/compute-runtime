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
        delete[] elfBinary;
        elfBinarySize = 0;

        elfBinary = new char[binarySize];

        elfBinarySize = binarySize;
        memcpy_s(elfBinary, elfBinarySize, pBinary, binarySize);
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
    cl_int retVal = CL_SUCCESS;
    CLElfLib::E_EH_TYPE headerType;
    CLElfLib::CElfWriter *pElfWriter = nullptr;

    if (isProgramBinaryResolved == false) {
        delete[] elfBinary;
        elfBinary = nullptr;
        elfBinarySize = 0;

        switch (programBinaryType) {
        case CL_PROGRAM_BINARY_TYPE_EXECUTABLE:
            headerType = CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE;

            if (!genBinary || !genBinarySize) {
                retVal = CL_INVALID_BINARY;
            }
            break;

        case CL_PROGRAM_BINARY_TYPE_LIBRARY:
            headerType = CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY;

            if (!irBinary || !irBinarySize) {
                retVal = CL_INVALID_BINARY;
            }
            break;

        case CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT:
            headerType = CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_OBJECTS;

            if (!irBinary || !irBinarySize) {
                retVal = CL_INVALID_BINARY;
            }
            break;

        default:
            retVal = CL_INVALID_BINARY;
        }

        if (retVal == CL_SUCCESS) {
            pElfWriter = CLElfLib::CElfWriter::create(headerType, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

            if (pElfWriter) {
                CLElfLib::SSectionNode sectionNode;

                // Always add the options string
                sectionNode.Name = "BuildOptions";
                sectionNode.Type = CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS;
                sectionNode.pData = (char *)options.c_str();
                sectionNode.DataSize = (uint32_t)(strlen(options.c_str()) + 1);

                auto elfRetVal = pElfWriter->addSection(&sectionNode);

                if (elfRetVal) {
                    // Add the LLVM component if available
                    if (getIsSpirV()) {
                        sectionNode.Type = CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV;
                    } else {
                        sectionNode.Type = CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY;
                    }
                    if (headerType == CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY) {
                        sectionNode.Name = "Intel(R) OpenCL LLVM Archive";
                        sectionNode.pData = (char *)irBinary;
                        sectionNode.DataSize = (uint32_t)irBinarySize;
                        elfRetVal = pElfWriter->addSection(&sectionNode);
                    } else {
                        sectionNode.Name = "Intel(R) OpenCL LLVM Object";
                        sectionNode.pData = (char *)irBinary;
                        sectionNode.DataSize = (uint32_t)irBinarySize;
                        elfRetVal = pElfWriter->addSection(&sectionNode);
                    }
                }

                // Add the device binary if it exists
                if (elfRetVal && genBinary) {
                    sectionNode.Name = "Intel(R) OpenCL Device Binary";
                    sectionNode.Type = CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY;
                    sectionNode.pData = (char *)genBinary;
                    sectionNode.DataSize = (uint32_t)genBinarySize;

                    elfRetVal = pElfWriter->addSection(&sectionNode);
                }

                // Add the device debug data if it exists
                if (elfRetVal && (debugData != nullptr)) {
                    sectionNode.Name = "Intel(R) OpenCL Device Debug";
                    sectionNode.Type = CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_DEBUG;
                    sectionNode.pData = debugData;
                    sectionNode.DataSize = (uint32_t)debugDataSize;
                    elfRetVal = pElfWriter->addSection(&sectionNode);
                }

                if (elfRetVal) {
                    elfRetVal = pElfWriter->resolveBinary(elfBinary, elfBinarySize);
                }

                if (elfRetVal) {
                    elfBinary = new char[elfBinarySize];

                    elfRetVal = pElfWriter->resolveBinary(elfBinary, elfBinarySize);
                }

                if (elfRetVal) {
                    isProgramBinaryResolved = true;
                } else {
                    retVal = CL_INVALID_BINARY;
                }
            } else {
                retVal = CL_OUT_OF_HOST_MEMORY;
            }

            CLElfLib::CElfWriter::destroy(pElfWriter);
        }
    }
    return retVal;
}
} // namespace OCLRT
