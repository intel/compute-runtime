# Contribution guidelines

## Process overview

### 1. Patch creation

Start with a patch (we prefer smaller self-contained incremental changes vs. large blobs of code).
When adding new code, please also add corresponding unit level tests (ULT). Added ULTs should cover
all the decision points introduced by the commit and should fail if executed without the code changes.
Make sure it builds and passes _all_ ULT tests. For details about what compilers and build configurations
we expect, refer to instructions for [Ubuntu](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Ubuntu.md)
or [Centos](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Centos.md).
Make sure you adhere to our coding standard - this will be verified by clang-format and clang-tidy
(tool configuration is already included in NEO repository).

### 2. Certificate of origin
In order to get a clear contribution chain of trust we use the [signed-off-by language](https://01.org/community/signed-process) used by the Linux kernel project. 
Please make sure your commit message adheres to this guideline.

### 3. Patch submission

Create a pull request on github once you are confident that your changes are complete and fulfill
the requirements above. Make sure your commit message follows these rules:
* each line has 80 character limit
* title (first line) should be self-contained (i.e. make sense without looking at the body)
* additional description can be provided in the body
* title and body need to be separated by an empty line

### 4. Initial (cursory) review

One of NEO maintainers will do an initial (brief) review of your code. We will let you know if anything major is missing.

### 5. Verification

We'll double-check that your code meets all of our minimal quality expectations (for every commit - see [RELEASES.md](https://github.com/intel/compute-runtime/blob/master/documentation/RELEASES.md)).
When all the automated checks are confirmed to be passing, we will start actual code review process.

### 6. Code review

We'll make sure that your code fits within the architecture and design of NEO, is readable and maintainable. Please make sure to address our questions and concerns. 

### 7. Patch disposition

We reserve, upon conclusion of the code review, the right to do one of the following:
1. Merge the patch as submitted
2. Merge the patch (with modifications)
3. Reject the patch

If merged, you will be listed as patch author.
Your patch may be reverted later in case of major regression that was not detected prior to commit.

## Intel Employees

If you are an Intel Employee *and* you want to contribute to NEO as part of your regular job duties, please:
* Contact us in advance
* Make sure your github account is linked to your intel.com email address
