<!---

Copyright (C) 2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# WDDM GPU Paravirtualization support for WSL2

## Introduction

This document describes ingredients required in order to use Intel&reg; Graphics Compute Runtime for oneAPI Level Zero and OpenCL&trade; Driver
in WSL2 environment.

## Host OS

Windows 11 or Windows 10 with the [21H2 update](https://blogs.windows.com/windowsexperience/2021/07/15/introducing-the-next-feature-update-to-windows-10-21h2/).

## WSL kernel

Tested with [5.10.16.3](https://docs.microsoft.com/en-us/windows/wsl/kernel-release-notes#510163).

## Host Graphics Driver

Required driver package (30.0.100.9955) is available [here](https://www.intel.com/content/www/us/en/download/19344/intel-graphics-windows-dch-drivers.html).

## Guest (WSL2) Intel&reg; Graphics Compute Runtime for oneAPI Level Zero and OpenCL&trade; Driver

Support was enabled at https://github.com/intel/compute-runtime/commit/fad4ee7e246839c36c3f6b0e14ea0c79d9e4758a
and it is included in [21.30.20482](https://github.com/intel/compute-runtime/releases/tag/21.30.20482) and beyond - use [latest](https://github.com/intel/compute-runtime/releases) release for best experience.
