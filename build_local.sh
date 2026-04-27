#!/bin/bash
#
# Build the Intel compute-runtime Level Zero driver and its required
# dependencies (GmmLib, IGC) for a given release tag.
#
# The script checks whether system-installed GmmLib and IGC versions are
# compatible with the requested release. If they are too old or missing,
# it installs the correct versions to a local prefix (no sudo required).
#
# Usage:
#   ./build_local.sh <tag> [install_prefix]
#
# Examples:
#   ./build_local.sh 26.09.37435.1
#   ./build_local.sh 26.09.37435.1 $HOME/my_prefix
#
# The latest release tag can be found at:
#   https://github.com/intel/compute-runtime/releases/latest

# Exit immediately on error, treat unset variables as errors, and fail on
# any command in a pipeline that fails (not just the last one).
set -euo pipefail

# Parse arguments: the release tag is required, the install prefix is optional.
TAG="${1:?Usage: $0 <tag> [install_prefix]}"
PREFIX="${2:-$HOME/local}"
REPO_URL="https://github.com/intel/compute-runtime"
# All intermediate build artifacts go into a temporary directory that is
# printed at the end so the user can clean it up manually if desired.
WORKDIR="$(mktemp -d)"
NPROC="$(nproc)"

echo "=== Building compute-runtime tag: ${TAG} ==="
echo "=== Install prefix: ${PREFIX} ==="
echo "=== Work directory: ${WORKDIR} ==="
echo ""

# On exit (success or failure), remind the user where temporary files are.
cleanup() {
    echo ""
    echo "=== Temporary files in: ${WORKDIR} ==="
    echo "    Remove with: rm -rf ${WORKDIR}"
}
trap cleanup EXIT

# --------------------------------------------------------------------------
# Step 1: Clone compute-runtime at the requested tag
# --------------------------------------------------------------------------
# Shallow clone (--depth 1) is sufficient since we only need the source at
# this exact tag, not the full git history.
echo "=== [1/5] Cloning compute-runtime at tag ${TAG} ==="
SRCDIR="${WORKDIR}/compute-runtime"
git clone --depth 1 -b "${TAG}" "${REPO_URL}.git" "${SRCDIR}"

# --------------------------------------------------------------------------
# Step 2: Read dependency versions from manifest
# --------------------------------------------------------------------------
# Each compute-runtime release pins its dependency versions in
# manifests/manifest.yml. We extract:
#   - GmmLib "revision": a git tag like "intel-gmmlib-22.9.0"
#   - IGC "branch": a release branch like "releases/2.30.x"
# We try python3+PyYAML first (reliable), falling back to sed if unavailable.
echo ""
echo "=== [2/5] Reading dependency versions from manifest ==="
MANIFEST="${SRCDIR}/manifests/manifest.yml"

if [ ! -f "${MANIFEST}" ]; then
    echo "ERROR: manifest.yml not found at ${MANIFEST}" >&2
    exit 1
fi

# Extract GmmLib revision (a git tag, e.g. "intel-gmmlib-22.9.0")
GMMLIB_REV=$(python3 -c "
import yaml, sys
with open('${MANIFEST}') as f:
    m = yaml.safe_load(f)
print(m['components']['gmmlib']['revision'])
" 2>/dev/null || sed -n '/^  gmmlib:/,/^  [a-z]/{/revision:/s/.*revision: *//p}' "${MANIFEST}")

# Extract IGC branch (e.g. "releases/2.30.x") — used to find the matching
# prebuilt release on GitHub (see Step 4).
IGC_BRANCH=$(python3 -c "
import yaml, sys
with open('${MANIFEST}') as f:
    m = yaml.safe_load(f)
print(m['components']['igc']['branch'])
" 2>/dev/null || sed -n '/^  igc:/,/^  [a-z]/{/branch:/s/.*branch: *//p}' "${MANIFEST}")

echo "  GmmLib revision: ${GMMLIB_REV}"
echo "  IGC branch:      ${IGC_BRANCH}"

# Check if system-installed versions are already compatible.
# GmmLib: compare the installed pkg-config version against the manifest tag.
# IGC: compare the installed major.minor against the manifest branch.
SYSTEM_GMMLIB_VER=$(pkg-config --modversion igdgmm 2>/dev/null || echo "not found")
SYSTEM_IGC_VER=$(pkg-config --modversion igc-opencl 2>/dev/null || echo "not found")
# Extract the expected GmmLib version number from the tag (e.g. "intel-gmmlib-22.9.0" -> "22.9.0")
EXPECTED_GMMLIB_VER=$(echo "${GMMLIB_REV}" | sed 's/intel-gmmlib-//')
# Extract the expected IGC major.minor from the branch (e.g. "releases/2.32.x" -> "2.32")
EXPECTED_IGC_MINOR=$(echo "${IGC_BRANCH}" | sed 's|releases/||; s|\.x$||')

echo ""
echo "  System GmmLib:   ${SYSTEM_GMMLIB_VER} (need ${EXPECTED_GMMLIB_VER})"
echo "  System IGC:      ${SYSTEM_IGC_VER} (need ${EXPECTED_IGC_MINOR}.x)"

NEED_LOCAL_GMMLIB=true
NEED_LOCAL_IGC=true

# GmmLib: the pkg-config version uses a different scheme (e.g. 12.9.0 for
# intel-gmmlib-22.9.0). We cannot reliably compare them, so we always build
# GmmLib from source to be safe. If the user has already installed the correct
# version to the prefix, the build step will be fast (no recompilation).

# IGC: check if the installed major.minor matches the expected one.
if echo "${SYSTEM_IGC_VER}" | grep -q "^${EXPECTED_IGC_MINOR}\."; then
    echo "  -> System IGC matches the required version, will skip local install."
    NEED_LOCAL_IGC=false
fi

# --------------------------------------------------------------------------
# Step 3: Build and install GmmLib
# --------------------------------------------------------------------------
# GmmLib (Graphics Memory Management Library) provides the igfxfmid.h header
# that defines GPU family enums (GFXCORE_FAMILY, PRODUCT_FAMILY) required by
# compute-runtime at compile time. We build it from source since there are no
# prebuilt packages on GitHub, and install it into the local prefix.
echo ""
if [ "${NEED_LOCAL_GMMLIB}" = true ]; then
echo "=== [3/5] Building GmmLib (${GMMLIB_REV}) ==="
cd "${WORKDIR}"
git clone --depth 1 -b "${GMMLIB_REV}" https://github.com/intel/gmmlib.git
cd gmmlib
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${PREFIX}" ..
make -j"${NPROC}"
make install
echo "  GmmLib installed to ${PREFIX}"
else
echo "=== [3/5] Skipping GmmLib (system version is compatible) ==="
fi

# --------------------------------------------------------------------------
# Step 4: Download and install IGC prebuilt packages
# --------------------------------------------------------------------------
# IGC (Intel Graphics Compiler) is complex to build from source (requires
# LLVM 16, SPIRV-LLVM-Translator, opencl-clang, etc.), so we use prebuilt
# .deb packages from GitHub releases instead.
#
# The manifest specifies an IGC branch like "releases/2.30.x". We convert
# that to a version pattern ("v2.30.") and search the GitHub releases API
# for the first matching tag (e.g. "v2.30.1").
#
# Each IGC release provides four .deb packages for amd64:
#   - intel-igc-core:         runtime libraries (libigc.so, libiga64.so)
#   - intel-igc-opencl:       OpenCL frontend library (libigdfcl.so)
#   - intel-igc-core-devel:   development headers (CIF, IGC interfaces)
#   - intel-igc-opencl-devel: OpenCL dev headers and pkg-config file
#
# We extract them with dpkg-deb (no sudo needed) and copy the contents
# into the local prefix.
echo ""
if [ "${NEED_LOCAL_IGC}" = true ]; then
echo "=== [4/5] Installing IGC from prebuilt packages ==="

# Convert branch name to version prefix for matching against release tags.
# e.g. "releases/2.30.x" -> "2.30" -> match tags starting with "v2.30."
IGC_VERSION_PATTERN=$(echo "${IGC_BRANCH}" | sed 's|releases/||; s|\.x$||')
echo "  Looking for IGC release matching: v${IGC_VERSION_PATTERN}.*"

# Find IGC release tags matching the version pattern using git ls-remote
# (does not require GitHub API tokens or rate limits).
IGC_CANDIDATE_TAGS=$(git ls-remote --tags https://github.com/intel/intel-graphics-compiler.git \
    "refs/tags/v${IGC_VERSION_PATTERN}*" 2>/dev/null | \
    sed 's|.*refs/tags/||' | sort -rV)

if [ -z "${IGC_CANDIDATE_TAGS}" ]; then
    echo "ERROR: Could not find any IGC release tags matching v${IGC_VERSION_PATTERN}.*" >&2
    exit 1
fi

# Not all tags have prebuilt .deb packages (some are source-only releases).
# Try each candidate tag (newest first) until we find one with .deb assets.
# We extract filenames from the release page HTML which includes checksums
# with filenames. This avoids the GitHub REST API which has strict rate
# limits for unauthenticated requests.
IGC_DEB_DIR="${WORKDIR}/igc_debs"
mkdir -p "${IGC_DEB_DIR}"

IGC_TAG=""
DEB_FILENAMES=""
for candidate in ${IGC_CANDIDATE_TAGS}; do
    echo "  Checking ${candidate} for .deb packages..."
    PAGE_CONTENT=$(curl -sL "https://github.com/intel/intel-graphics-compiler/releases/tag/${candidate}")
    DEB_FILENAMES=$(echo "${PAGE_CONTENT}" | grep -oP '[a-z_\-0-9.+]+_amd64\.deb' | sort -u || true)
    if [ -n "${DEB_FILENAMES}" ]; then
        IGC_TAG="${candidate}"
        break
    fi
done

if [ -z "${IGC_TAG}" ]; then
    echo "ERROR: No IGC release with .deb packages found for v${IGC_VERSION_PATTERN}.*" >&2
    exit 1
fi

echo "  Found IGC release: ${IGC_TAG}"

# Construct download URLs from the tag and filenames.
ASSET_URLS=""
for fname in ${DEB_FILENAMES}; do
    ASSET_URLS="${ASSET_URLS} https://github.com/intel/intel-graphics-compiler/releases/download/${IGC_TAG}/${fname}"
done

echo "  Downloading IGC packages..."
for url in ${ASSET_URLS}; do
    echo "    $(basename "${url}")"
    wget -q -P "${IGC_DEB_DIR}" "${url}"
done

# Extract all .deb packages into a staging directory, then copy headers
# and libraries into the local prefix.
echo "  Extracting packages to ${PREFIX}..."
IGC_EXTRACT="${WORKDIR}/igc_extract"
mkdir -p "${IGC_EXTRACT}"
for deb in "${IGC_DEB_DIR}"/*.deb; do
    dpkg-deb -x "${deb}" "${IGC_EXTRACT}"
done

mkdir -p "${PREFIX}/include" "${PREFIX}/lib"
if [ -d "${IGC_EXTRACT}/usr/local/include" ]; then
    cp -r "${IGC_EXTRACT}"/usr/local/include/* "${PREFIX}/include/"
fi
if [ -d "${IGC_EXTRACT}/usr/local/lib" ]; then
    cp -r "${IGC_EXTRACT}"/usr/local/lib/* "${PREFIX}/lib/"
fi

# The IGC pkg-config file ships with prefix=/usr/local hardcoded.
# Rewrite it to point to our local prefix so cmake/pkg-config can find it.
IGC_PC="${PREFIX}/lib/pkgconfig/igc-opencl.pc"
if [ -f "${IGC_PC}" ]; then
    sed -i "s|^prefix=.*|prefix=${PREFIX}|" "${IGC_PC}"
    echo "  Fixed pkg-config prefix in igc-opencl.pc"
fi

echo "  IGC ${IGC_TAG} installed to ${PREFIX}"
else
echo "=== [4/5] Skipping IGC (system version ${SYSTEM_IGC_VER} is compatible) ==="
IGC_TAG="system-${SYSTEM_IGC_VER}"
fi

# --------------------------------------------------------------------------
# Step 5: Build compute-runtime
# --------------------------------------------------------------------------
# Configure and build the driver with cmake. Key options:
#   -DNEO_SKIP_UNIT_TESTS=1  : skip building test binaries (much faster)
#   -DCMAKE_PREFIX_PATH      : tells cmake where to find our local GmmLib/IGC
#   -DCOMPILE_BUILT_INS=OFF  : skip compiling built-in GPU kernels. This is
#       needed because ocloc loads IGC at runtime via dlopen("libigdfcl.so"),
#       which resolves to the old system-installed IGC rather than our local
#       one. Replacing the system IGC would require sudo. To enable built-in
#       kernel compilation, set LD_LIBRARY_PATH to include ${PREFIX}/lib.
#
# PKG_CONFIG_PATH is set so that cmake's pkg-config lookups find our local
# igc-opencl.pc and igdgmm.pc before any system-installed versions.
# If local deps were installed, we also set CMAKE_PREFIX_PATH and disable
# built-in kernel compilation (COMPILE_BUILT_INS=OFF).
echo ""
echo "=== [5/5] Building compute-runtime ==="
cd "${SRCDIR}"
mkdir -p build && cd build

CMAKE_EXTRA_ARGS=""
if [ "${NEED_LOCAL_GMMLIB}" = true ] || [ "${NEED_LOCAL_IGC}" = true ]; then
    export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    CMAKE_EXTRA_ARGS="-DCMAKE_PREFIX_PATH=${PREFIX} -DCOMPILE_BUILT_INS=OFF"
fi

cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DNEO_SKIP_UNIT_TESTS=1 \
    ${CMAKE_EXTRA_ARGS} \
    ..
make -j"${NPROC}"

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
# Print the versions used and the paths to the built driver libraries.
# Also show the exact command needed to test the driver with sycl-ls:
#   - LD_LIBRARY_PATH ensures the loader finds our built libs and local IGC/GmmLib
#   - OVERRIDEZEDRIVERSEARCH tells the Level Zero loader to look for the
#     driver (libze_intel_gpu.so) in our build directory instead of the
#     system install path
echo ""
echo "=========================================="
echo "  Build complete!"
echo "=========================================="
echo ""
echo "  Tag:     ${TAG}"
echo "  GmmLib:  ${GMMLIB_REV}"
echo "  IGC:     ${IGC_TAG}"
echo ""
echo "  Built artifacts:"
BINDIR="${SRCDIR}/build/bin"
for lib in "${BINDIR}"/libze_intel_gpu.so* "${BINDIR}"/libigdrcl.so* "${BINDIR}"/ocloc-*; do
    if [ -e "${lib}" ] && [ ! -L "${lib}" ]; then
        echo "    ${lib}"
    fi
done
echo ""
echo "  To use with sycl-ls:"
echo "    LD_LIBRARY_PATH=${BINDIR}:${PREFIX}/lib:\$LD_LIBRARY_PATH \\"
echo "      OVERRIDEZEDRIVERSEARCH=${BINDIR} \\"
echo "      sycl-ls"
echo ""
