/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#pragma warning(disable : 4996)
#else
#include <unistd.h>
#endif

#include <iostream>
#include <vector>

#ifdef _WIN32
const char *fSeparator = "\\";
#elif defined(__linux__)
const char *fSeparator = "/";
#endif

std::string getRunPath(char *argv0) {
    std::string res(argv0);

    auto pos = res.rfind(fSeparator);
    if (pos != std::string::npos)
        res = res.substr(0, pos);

    if (res == "." || pos == std::string::npos) {
        char *cwd;
#if defined(__linux__)
        cwd = getcwd(nullptr, 0);
#else
        cwd = _getcwd(nullptr, 0);
#endif
        res = cwd;
        free(cwd);
    }

    return res;
}

int main(int argc, char *argv[], char **envp) {
    char *argv2[] = {NULL, NULL, NULL};
    auto path = getRunPath(argv[0]);
    path += fSeparator;
    path += "zello_bindless_kernel";
    argv2[0] = const_cast<char *>(path.c_str());
    const char verbose[] = "--verbose";
    argv2[1] = const_cast<char *>(verbose);
    std::vector<const char *> allEnv;

    for (auto env = envp; *env != nullptr; env++) {
        allEnv.push_back(*env);
    }
    allEnv.push_back("UseExternalAllocatorForSshAndDsh=1");
    allEnv.push_back("UseBindlessMode=1");
    allEnv.push_back("PrintDebugSettings=1");
    allEnv.push_back(nullptr);

    std::cout << "\nRunning " << argv2[0] << " with UseExternalAllocatorForSshAndDsh=1 UseBindlessMode=1..." << std::endl;

    execve(argv2[0], argv2, const_cast<char **>(allEnv.data()));

    path = getRunPath(argv[0]);
    path += fSeparator;
    path += "zello_scratch";
    argv2[0] = const_cast<char *>(path.c_str());

    std::cout << "\nRunning " << argv2[0] << " with UseExternalAllocatorForSshAndDsh=1 UseBindlessMode=1..." << std::endl;

    execve(argv2[0], argv2, const_cast<char **>(allEnv.data()));

    allEnv.clear();
    for (auto env = envp; *env != nullptr; env++) {
        allEnv.push_back(*env);
    }
    allEnv.push_back("UseExternalAllocatorForSshAndDsh=0");
    allEnv.push_back("PrintDebugSettings=1");
    allEnv.push_back(nullptr);

    path = getRunPath(argv[0]);
    path += fSeparator;
    path += "zello_bindless_kernel";
    argv2[0] = const_cast<char *>(path.c_str());

    std::cout << "\nRunning " << argv2[0] << " with UseExternalAllocatorForSshAndDsh=0..." << std::endl;

    execve(argv2[0], argv2, const_cast<char **>(allEnv.data()));

    return -1;
}
