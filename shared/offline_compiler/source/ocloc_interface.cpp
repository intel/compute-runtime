/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_interface.h"

#include "shared/offline_compiler/source/decoder/binary_decoder.h"
#include "shared/offline_compiler/source/decoder/binary_encoder.h"
#include "shared/offline_compiler/source/decoder/zebin_manipulator.h"
#include "shared/offline_compiler/source/multi_command.h"
#include "shared/offline_compiler/source/ocloc_concat.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/offline_compiler/source/ocloc_validator.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/offline_linker.h"
#include "shared/offline_compiler/source/utilities/safety_caller.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"

#include <memory>

namespace Ocloc {
using namespace NEO;

void printOclocCmdLine(const std::vector<std::string> &args) {
    auto areQuotesRequired = [](const std::string_view &argName) -> bool {
        return argName == "-options" || argName == "-internal_options";
    };
    printf("Command was:");
    bool useQuotes = false;
    for (auto &currArg : args) {
        if (useQuotes) {
            printf(" \"%s\"", currArg.c_str());
            useQuotes = false;
        } else {
            printf(" %s", currArg.c_str());
            useQuotes = areQuotesRequired(currArg.c_str());
        }
    }
    printf("\n");
}

void printHelp(OclocArgHelper *helper) {
    const char *help = R"===(ocloc is a tool for managing Intel Compute GPU device binary format.
It can be used for generation (as part of 'compile' command) as well as
manipulation (decoding/modifying - as part of 'disasm'/'asm' commands) of such
binary files.
Intel Compute GPU device binary is a format used by Intel Compute GPU runtime
(aka NEO). Intel Compute GPU runtime will return this binary format when queried
using clGetProgramInfo(..., CL_PROGRAM_BINARIES, ...). It will also honor
this format as input to clCreateProgramWithBinary function call.
ocloc does not require Intel GPU device to be present in the system nor does it
depend on Intel Compute GPU runtime driver to be installed. It does however rely
on the same set of compilers (IGC, common_clang) as the runtime driver.

Usage: ocloc [--help] <command> [<command_args>]
Available commands are listed below.
Use 'ocloc <command> --help' to get help about specific command.

Commands:
  compile               Compiles input to Intel Compute GPU device binary.
  link                  Links several IR files.
  disasm                Disassembles Intel Compute GPU device binary.
  asm                   Assembles Intel Compute GPU device binary.
  multi                 Compiles multiple files using a config file.
  validate              Validates Intel Compute GPU device binary.
  query                 Extracts versioning info.
  ids                   Return matching versions <major>.<minor>.<revision>.
  concat                Concatenates multiple fat binaries.

Default command (when none provided) is 'compile'.

Examples:
  Compile file to Intel Compute GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl

  Link two SPIR-V files.
    ocloc link -file sample1.spv -file sample2.spv -out_format LLVM_BC -out samples_merged.llvm_bc

  Disassemble Intel Compute GPU device binary
    ocloc disasm -file source_file_Gen9core.bin

  Assemble to Intel Compute GPU device binary (after above disasm)
    ocloc asm -out reassembled.bin

  Validate Intel Compute GPU device binary
    ocloc validate -file source_file_Gen9core.bin

  Extract driver version
    ocloc query OCL_DRIVER_VERSION

  Return matching version for an acronym
    ocloc ids dg1

  Concatenate fat binaries
    ocloc concat <fat binary> <fat binary> ... [-out <concatenated fat binary name>]
}
)===";
    helper->printf("%s", help);
}

void printOclocOptionsReadFromFile(OfflineCompiler *pCompiler) {
    if (pCompiler) {
        std::string options = pCompiler->getOptionsReadFromFile();
        if (options != "") {
            printf("Compiling options read from file were:\n%s\n", options.c_str());
        }

        std::string internalOptions = pCompiler->getInternalOptionsReadFromFile();
        if (internalOptions != "") {
            printf("Internal options read from file were:\n%s\n", internalOptions.c_str());
        }
    }
}

namespace Commands {

int compile(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    if (NEO::requestedFatBinary(args, argHelper)) {
        return NEO::buildFatBinary(args, argHelper);
    }

    int retVal = OclocErrorCode::SUCCESS;

    std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(args.size(), args, true, retVal, argHelper)};
    if (retVal == OclocErrorCode::SUCCESS) {
        retVal = buildWithSafetyGuard(pCompiler.get());

        std::string buildLog = pCompiler->getBuildLog();
        if (buildLog.empty() == false) {
            argHelper->printf("%s\n", buildLog.c_str());
        }

        if (retVal == OclocErrorCode::SUCCESS) {
            if (!pCompiler->isQuiet())
                argHelper->printf("Build succeeded.\n");
        } else {
            argHelper->printf("Build failed with error code: %d\n", retVal);
        }
    }

    if (retVal != OclocErrorCode::SUCCESS) {
        printOclocOptionsReadFromFile(pCompiler.get());
        printOclocCmdLine(args);
    }
    return retVal;
};

int link(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    int createResult{OclocErrorCode::SUCCESS};
    const auto linker{OfflineLinker::create(args.size(), args, createResult, argHelper)};
    const auto linkingResult{linkWithSafetyGuard(linker.get())};

    const auto buildLog = linker->getBuildLog();
    if (!buildLog.empty()) {
        argHelper->printf("%s\n", buildLog.c_str());
    }

    if (createResult == OclocErrorCode::SUCCESS && linkingResult == OclocErrorCode::SUCCESS) {
        argHelper->printf("Linker execution has succeeded!\n");
    }

    return createResult | linkingResult;
};

int disassemble(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    const auto binaryFormat = ZebinManipulator::getBinaryFormatForDisassemble(argHelper, args);
    auto decode = [&args](auto &decoder) -> int {
        int retVal = decoder.validateInput(args);
        if (decoder.showHelp) {
            decoder.printHelp();
            return OclocErrorCode::SUCCESS;
        }
        return (retVal == OclocErrorCode::SUCCESS) ? decoder.decode() : retVal;
    };

    if (binaryFormat == ZebinManipulator::BinaryFormats::PatchTokens) {
        BinaryDecoder disasm(argHelper);
        return decode(disasm);

    } else if (binaryFormat == ZebinManipulator::BinaryFormats::Zebin32b) {
        ZebinManipulator::ZebinDecoder<Elf::EI_CLASS_32> decoder(argHelper);
        return decode(decoder);
    } else {
        ZebinManipulator::ZebinDecoder<Elf::EI_CLASS_64> decoder(argHelper);
        return decode(decoder);
    }
}

int assemble(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    const auto binaryFormat = ZebinManipulator::getBinaryFormatForAssemble(argHelper, args);
    auto encode = [&args](auto &encoder) -> int {
        int retVal = encoder.validateInput(args);
        if (encoder.showHelp) {
            encoder.printHelp();
            return OclocErrorCode::SUCCESS;
        }
        return (retVal == OclocErrorCode::SUCCESS) ? encoder.encode() : retVal;
    };
    if (binaryFormat == ZebinManipulator::BinaryFormats::PatchTokens) {
        BinaryEncoder assembler(argHelper);
        return encode(assembler);
    } else if (binaryFormat == ZebinManipulator::BinaryFormats::Zebin32b) {
        ZebinManipulator::ZebinEncoder<Elf::EI_CLASS_32> encoder(argHelper);
        return encode(encoder);
    } else {
        ZebinManipulator::ZebinEncoder<Elf::EI_CLASS_64> encoder(argHelper);
        return encode(encoder);
    }
}

int multi(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    int retValue = OclocErrorCode::SUCCESS;
    std::unique_ptr<MultiCommand> pMulti{(MultiCommand::create(args, retValue, argHelper))};
    return retValue;
}

int validate(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    return Ocloc::validate(args, argHelper);
}

int query(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    return OfflineCompiler::query(args.size(), args, argHelper);
}

int ids(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    return OfflineCompiler::queryAcronymIds(args.size(), args, argHelper);
}

int concat(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    auto arConcat = NEO::OclocConcat(argHelper);
    auto error = arConcat.initialize(args);
    if (OclocErrorCode::SUCCESS != error) {
        arConcat.printHelp();
        return error;
    }

    error = arConcat.concatenate();
    return error;
}

} // namespace Commands
} // namespace Ocloc
