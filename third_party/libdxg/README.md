# Introduction

This repository hosts
- the official WDDM (Windows Display Driver Model) kernel API headers. These headers are
 made available under the MIT license rather than the traditional Windows SDK license.
 - The source of the libdxg shared library for WSL, which translates the WDDM interface calls to the /dev/dxg drive IOCTLs.
 - a sample project https://github.com/microsoft/libdxgtest, which shows how to use libdxg in a Meson project.

# Getting Started
##	Directory structure
- /meson.build - Meson build files for inclusion using sub-bproject/wrap.
- /src/d3dkmt-wsl.cpp - source file for libdxg.so
- /include/*.h WDDM header files to install or copy to an include directory
- /subprojects/DirectX-Headers.warp - Meson dependency for DirectX headers

##	Software dependencies

The libdxg code depends on uisng wsl/winadapter.h from https://github.com/microsoft/DirectX-Headers

##	API references

The WDDM API is described on MSDN https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/

# Ways to consume

- Manually: Just copy the headers somewhere and point your project at them.
- Meson subproject/wrap: Add this entire project as a subproject of your larger project, and use subproject or dependency to consume it.
- Pkg-config: Use Meson to build this project, and the resulting installed package can be found via pkg-config.

# Build and Test

Meson build system is used to compile and build the shared library and install header files.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
