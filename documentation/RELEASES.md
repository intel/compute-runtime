# Quality expectations

## Every commit - must ...

* Build under Linux - using clang (6.0, planning switch to 7.0) and multiple versions of gcc (5.2 ... 8.2)

* Build under Windows (this is currently a requirement that cannot be verified externally)

* Pass ULTs for all supported platforms

* Pass clang-format check with the configuration contained within repository

* Pass clang-tidy check with the configuration contained within repository

* Pass sanity testing (test content recommendation for the external community will be provided in the future)

## Weekly

* Once a week, we run extended cycle on a selected driver.

* When the extended cycle passes, the corresponding commit on github is tagged using the format yy.ww.bbbb (yy - year, ww - work week, bbbb - incremental build number).

* Typically for weekly tags we will post a binary release (e.g. deb).

* Quality level of the driver (per platform) will be provided in the Release Notes.
