/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <exception>
#include <string>
#include <vector>

void addSlash(std::string &path);

std::vector<char> readBinaryFile(const std::string &fileName);

void readFileToVectorOfStrings(std::vector<std::string> &lines, const std::string &fileName, bool replaceTabs = false);

size_t findPos(const std::vector<std::string> &lines, const std::string &whatToFind);
