/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>
#include <vector>
#include <exception>

void addSlash(std::string &path);

std::vector<char> readBinaryFile(const std::string &fileName);

void readFileToVectorOfStrings(std::vector<std::string> &lines, const std::string &fileName, bool replaceTabs = false);

size_t findPos(const std::vector<std::string> &lines, const std::string &whatToFind);
