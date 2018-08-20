/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "offline_compiler/decoder/binary_decoder.h"

struct MockDecoder : public BinaryDecoder {
    MockDecoder() : BinaryDecoder("", "", ""){};
    MockDecoder(const std::string &file, const std::string &patch, const std::string &dump)
        : BinaryDecoder(file, patch, dump){};
    using BinaryDecoder::binaryFile;
    using BinaryDecoder::decode;
    using BinaryDecoder::getSize;
    using BinaryDecoder::kernelHeader;
    using BinaryDecoder::parseTokens;
    using BinaryDecoder::patchTokens;
    using BinaryDecoder::pathToDump;
    using BinaryDecoder::pathToPatch;
    using BinaryDecoder::processBinary;
    using BinaryDecoder::processKernel;
    using BinaryDecoder::programHeader;
    using BinaryDecoder::readPatchTokens;
    using BinaryDecoder::readStructFields;
};
