/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "iga_wrapper.h"

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/os_inc_base.h"
#include "runtime/os_interface/os_library.h"

#include "helper.h"
#include "igfxfmid.h"
#include "translate_platform_base.h"

#include <memory>

struct IgaLibrary {
    pIGAAssemble assemble = nullptr;
    pIGAContextCreate contextCreate = nullptr;
    pIGAContextGetErrors contextGetErrors = nullptr;
    pIGAContextGetWarnings contextGetWarnings = nullptr;
    pIGAContextRelease contextRelease = nullptr;
    pIGADisassemble disassemble = nullptr;
    pIGAStatusToString statusToString = nullptr;
    iga_context_options_t OptsContext = {};

    std::unique_ptr<NEO::OsLibrary> library;

    bool isLoaded() {
        return library != nullptr;
    }
};

struct IgaWrapper::Impl {
    iga_gen_t igaGen = IGA_GEN_INVALID;
    IgaLibrary igaLib;

    void loadIga() {
        IgaLibrary iga;
        iga.OptsContext.cb = sizeof(igaLib.OptsContext);
        iga.OptsContext.gen = igaGen;

#define STR2(X) #X
#define STR(X) STR2(X)
        iga.library.reset(NEO::OsLibrary::load(STR(IGA_LIBRARY_NAME)));
        if (iga.library == nullptr) {
            return;
        }

#define LOAD_OR_ERROR(MEMBER, FUNC_NAME)                                                                            \
    if (nullptr == (iga.MEMBER = reinterpret_cast<decltype(iga.MEMBER)>(iga.library->getProcAddress(FUNC_NAME)))) { \
        printf("Warning : Couldn't find %s in %s\n", FUNC_NAME, STR(IGA_LIBRARY_NAME));                             \
        return;                                                                                                     \
    }

        LOAD_OR_ERROR(assemble, IGA_ASSEMBLE_STR);
        LOAD_OR_ERROR(contextCreate, IGA_CONTEXT_CREATE_STR);
        LOAD_OR_ERROR(contextGetErrors, IGA_CONTEXT_GET_ERRORS_STR);
        LOAD_OR_ERROR(contextGetWarnings, IGA_CONTEXT_GET_WARNINGS_STR);
        LOAD_OR_ERROR(contextRelease, IGA_CONTEXT_RELEASE_STR);
        LOAD_OR_ERROR(disassemble, IGA_DISASSEMBLE_STR);
        LOAD_OR_ERROR(statusToString, IGA_STATUS_TO_STRING_STR);

#undef LOAD_OR_ERROR
#undef STR
#undef STR2

        this->igaLib = std::move(iga);
    }
};

IgaWrapper::IgaWrapper()
    : pimpl(new Impl()) {
}

IgaWrapper::~IgaWrapper() = default;

bool IgaWrapper::tryDisassembleGenISA(const void *kernelPtr, uint32_t kernelSize, std::string &out) {
    if (false == tryLoadIga()) {
        messagePrinter->printf("Warning: couldn't load iga - kernel binaries won't be disassembled.\n");
        return false;
    }

    iga_context_t context;
    iga_disassemble_options_t disassembleOptions = IGA_DISASSEMBLE_OPTIONS_INIT();
    iga_status_t stat;

    stat = pimpl->igaLib.contextCreate(&pimpl->igaLib.OptsContext, &context);
    if (stat != 0) {
        messagePrinter->printf("Error while creating IGA Context! Error msg: %s", pimpl->igaLib.statusToString(stat));
        return false;
    }

    char kernelText = '\0';
    char *pKernelText = &kernelText;

    stat = pimpl->igaLib.disassemble(context, &disassembleOptions, kernelPtr, kernelSize, nullptr, nullptr, &pKernelText);
    if (stat != 0) {
        messagePrinter->printf("Error while disassembling with IGA!\nStatus msg: %s\n", pimpl->igaLib.statusToString(stat));
        const iga_diagnostic_t *errors;
        uint32_t size = 100;
        pimpl->igaLib.contextGetErrors(context, &errors, &size);
        if (errors != nullptr) {
            messagePrinter->printf("Errors: %s\n", errors->message);
        }
        pimpl->igaLib.contextRelease(context);
        return false;
    }

    const iga_diagnostic_t *warnings;
    uint32_t warningsSize = 100;
    pimpl->igaLib.contextGetWarnings(context, &warnings, &warningsSize);
    if (warningsSize > 0 && warnings != nullptr) {
        messagePrinter->printf("Warnings: %s\n", warnings->message);
    }

    out = pKernelText;
    pimpl->igaLib.contextRelease(context);
    return true;
}

bool IgaWrapper::tryAssembleGenISA(const std::string &inAsm, std::string &outBinary) {
    if (false == tryLoadIga()) {
        messagePrinter->printf("Warning: couldn't load iga - kernel binaries won't be assembled.\n");
        return false;
    }

    iga_context_t context;
    iga_status_t stat;
    iga_assemble_options_t assembleOptions = IGA_ASSEMBLE_OPTIONS_INIT();

    stat = pimpl->igaLib.contextCreate(&pimpl->igaLib.OptsContext, &context);
    if (stat != 0) {
        messagePrinter->printf("Error while creating IGA Context! Error msg: %s", pimpl->igaLib.statusToString(stat));
        return false;
    }

    uint32_t size = 0;
    void *pOutput = nullptr;
    stat = pimpl->igaLib.assemble(context, &assembleOptions, inAsm.c_str(), &pOutput, &size);
    if (stat != 0) {
        messagePrinter->printf("Error while assembling with IGA!\nStatus msg: %s\n", pimpl->igaLib.statusToString(stat));

        const iga_diagnostic_t *errors;
        uint32_t size = 100;
        pimpl->igaLib.contextGetErrors(context, &errors, &size);
        if (errors != nullptr) {
            messagePrinter->printf("Errors: %s\n", errors->message);
        }

        pimpl->igaLib.contextRelease(context);
        return false;
    }

    const iga_diagnostic_t *warnings;
    uint32_t context_size;
    pimpl->igaLib.contextGetWarnings(context, &warnings, &context_size);
    if (context_size > 0 && warnings != nullptr) {
        messagePrinter->printf("Warnings: %s\n", warnings->message);
    }

    outBinary.assign(reinterpret_cast<char *>(pOutput), reinterpret_cast<char *>(pOutput) + size);

    pimpl->igaLib.contextRelease(context);
    return true;
}

bool IgaWrapper::tryLoadIga() {
    if (false == pimpl->igaLib.isLoaded()) {
        pimpl->loadIga();
    }
    return pimpl->igaLib.isLoaded();
}

void IgaWrapper::setGfxCore(GFXCORE_FAMILY core) {
    if (pimpl->igaGen == IGA_GEN_INVALID) {
        pimpl->igaGen = translateToIgaGen(core);
    }
}

void IgaWrapper::setProductFamily(PRODUCT_FAMILY product) {
    if (pimpl->igaGen == IGA_GEN_INVALID) {
        pimpl->igaGen = translateToIgaGen(product);
    }
}

bool IgaWrapper::isKnownPlatform() const {
    return pimpl->igaGen != IGA_GEN_INVALID;
}
