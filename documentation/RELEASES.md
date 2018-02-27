# Quality expectations

## Every commit - must ...

* Build under Linux - using multiple versions of gcc (4.8 ... 7.3) and clang (4.0.1)

* Build under Windows (this is currently a requirement that cannot be verified externally)

* Pass ULTs for all supported platforms

* Pass clang-format check with the configuration contained within repository

* Pass clang-tidy check with the configuration contained within repository

* Pass sanity testing (test content recommendation for the external community will be provided in the future)

## Weekly

* Once a week, we run extended cycle on a selected driver. When the extended cycle passes, the corresponding commit on github is tagged (e.g. "2018ww08"). Such version is considered recommended for the community use ("latest good")

* For selected weekly tags, we may choose to release binaries (deb, rpm, tarball) - those will usually be considered a Beta driver.

## Monthly / quarterly releases

* For major _driver_ releases, OpenCL driver is expected to be bundled with Media and/or MESA drivers. Cadence and timeline will be coordinated with our OTC colleagues.

* The driver releases (weekly/monthly/quarterly) will then be bundled with software products (Intel(R) Computer Vision SDK, Intel(R) SDK for OpenCL(TM) Applications, Intel(R) Media Server Studio, etc.) as appropriate.
