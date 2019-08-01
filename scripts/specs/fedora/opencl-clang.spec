%global cclang_commit_id 41cad395859684b18e762ca4a2c713c2fa349622
%global spirv_llvm_commit_id 83298e3c9b124486c16d0fde54c764a6c5a2b554
%global package_version 8.0.72
%global package_release 1

Name:       intel-opencl-clang
Version:    %{package_version}
Release:    %{package_release}%{?dist}
Summary:    Intel(R) OpenCL(TM) Clang

Group:      System Environment/Libraries
License:    MIT
URL:        https://github.com/intel/opencl-clang
Source0:    https://github.com/intel/opencl-clang/archive/%{cclang_commit_id}/intel-opencl-clang.tar.gz
Source1:    https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/%{spirv_llvm_commit_id}/spirv-llvm-translator.tar.gz

BuildRequires: git cmake clang gcc-c++ make patch llvm-devel clang-devel pkg-config python2 procps dos2unix

%description
Common clang is a thin wrapper library around clang. Common clang has OpenCL-oriented API and is capable to compile OpenCL C kernels to SPIR-V modules.

%package        devel
Summary:        Development files Intel(R) OpenCL(TM) Clang
Requires:       %{name} = %{version}-%{release}

%description devel


%clean
echo "==== CLEAN ===="

%prep
echo "==== PREP ===="

mkdir opencl-clang spirv-llvm
tar xzf $RPM_SOURCE_DIR/intel-opencl-clang.tar.gz -C opencl-clang --strip-components=1
tar xzf $RPM_SOURCE_DIR/spirv-llvm-translator.tar.gz -C spirv-llvm --strip-components=1

%build
echo "==== BUILD ===="

mkdir build_cc build_s

pushd spirv-llvm
dos2unix ../opencl-clang/patches/spirv/0001-Update-LowerOpenCL-pass-to-handle-new-blocks-represn.patch
dos2unix ../opencl-clang/patches/spirv/0002-Translation-of-llvm.dbg.declare-in-case-the-local-va.patch

patch -p1 < ../opencl-clang/patches/spirv/0001-Update-LowerOpenCL-pass-to-handle-new-blocks-represn.patch
patch -p1 < ../opencl-clang/patches/spirv/0002-Translation-of-llvm.dbg.declare-in-case-the-local-va.patch
popd

pushd build_s
cmake ../spirv-llvm -DCMAKE_INSTALL_PREFIX=install -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release
make llvm-spirv -j8 install
popd

pushd build_cc
cmake ../opencl-clang -DCOMMON_CLANG_LIBRARY_NAME=opencl-clang -DLLVMSPIRV_INCLUDED_IN_LLVM=OFF -DSPIRV_TRANSLATOR_DIR=../build_s/install -DLLVM_NO_DEAD_STRIP=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX='/usr'
make -j8
popd

%install
echo "==== INSTALL ===="
cd build_cc
make install DESTDIR=$RPM_BUILD_ROOT

%files

/usr/lib64/libopencl-clang.so.8

%files devel

/usr/lib64/libopencl-clang.so
/usr/include/cclang/common_clang.h

%doc

%changelog
