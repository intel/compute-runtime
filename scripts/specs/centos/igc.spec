%global igc_commit_id 8b0b9d18e0bc602b732fd50a920b203c23ef630f
%global cclang_commit_id 41cad395859684b18e762ca4a2c713c2fa349622
%global spirv_llvm_commit_id 83298e3c9b124486c16d0fde54c764a6c5a2b554
%global llvm_commit_id release_80
%global clang_commit_id release_80
%global llvm_patches_id 605739e09f4b5b176a93613c43727fd37fd77e71
%global major_version 1
%global minor_version 0
%global patch_version 2456
%global package_release 1

Name:       intel-igc
Version:    %{major_version}.%{minor_version}.%{patch_version}
Release:    %{package_release}%{?dist}
Summary:    Intel(R) Graphics Compiler for OpenCL(TM)

Group:      System Environment/Libraries
License:    MIT
URL:        https://github.com/intel/intel-graphics-compiler
Source0:    https://github.com/intel/intel-graphics-compiler/archive/%{igc_commit_id}/igc.tar.gz
Source1:    https://github.com/intel/opencl-clang/archive/%{cclang_commit_id}/intel-opencl-clang.tar.gz
Source2:    https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/%{spirv_llvm_commit_id}/spirv-llvm-translator.tar.gz
Source3:    https://github.com/llvm-mirror/llvm/archive/%{llvm_commit_id}/llvm-80.tar.gz
Source4:    https://github.com/llvm-mirror/clang/archive/%{clang_commit_id}/clang-80.tar.gz
Source5:    https://github.com/intel/llvm-patches/archive/%{llvm_patches_id}/llvm-patches.tar.gz

BuildRequires: centos-release-scl epel-release
BuildRequires: devtoolset-4-gcc-c++ cmake3
BuildRequires: git make patch pkgconfig python2 procps bison flex

%description
Intel(R) Graphics Compiler for OpenCL(TM).

%package       core
Summary:       Intel(R) Graphics Compiler Core

%description   core

%package       opencl
Summary:       Intel(R) Graphics Compiler Frontend
Requires:      %{name}-core = %{version}-%{release}

%description   opencl

%package       opencl-devel
Summary:       Intel(R) Graphics Compiler development package
Requires:      %{name}-opencl = %{version}-%{release}

%description   opencl-devel



%clean
echo "==== CLEAN ===="

%prep
echo "==== PREP ===="

mkdir llvm_source
tar xzf $RPM_SOURCE_DIR/llvm-80.tar.gz -C llvm_source --strip-components=1

cd llvm_source/tools
mkdir clang
tar xzf $RPM_SOURCE_DIR/clang-80.tar.gz -C clang --strip-components=1
cd clang
git init
git add .
git config --local user.name "pbuilder"
git config --local user.email "pbuilder@intel.com"
git commit -a -m "add sources"

cd ../../projects/
mkdir opencl-clang llvm-spirv
tar xzf $RPM_SOURCE_DIR/intel-opencl-clang.tar.gz -C opencl-clang --strip-components=1
cd opencl-clang
sed -i "s/ccfe04576c13497b9c422ceef0b6efe99077a392/master/" CMakeLists.txt
sed -i "s/83298e3c9b124486c16d0fde54c764a6c5a2b554/master/" CMakeLists.txt
cd ..

tar xzf $RPM_SOURCE_DIR/spirv-llvm-translator.tar.gz -C llvm-spirv --strip-components=1
cd llvm-spirv
git init
git add .
git config --local user.name "pbuilder"
git config --local user.email "pbuilder@intel.com"
git commit -a -m "add sources"
cd ../../..

mkdir igc
tar xzf $RPM_SOURCE_DIR/igc.tar.gz -C igc --strip-components=1

mkdir llvm_patches
tar xzf $RPM_SOURCE_DIR/llvm-patches.tar.gz -C llvm_patches --strip-components=1


%build
echo "==== BUILD ===="
mkdir build

pushd build
scl enable devtoolset-4 "cmake3 ../igc -Wno-dev -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr  -DCOMMON_CLANG_LIBRARY_NAME=opencl-clang -DIGC_PREFERRED_LLVM_VERSION=8.0.0"
scl enable devtoolset-4 "make -j `nproc`"
popd


%install
echo "==== INSTALL ===="
cd build
make -j`nproc` install DESTDIR=$RPM_BUILD_ROOT

rm -fv $RPM_BUILD_ROOT/usr/bin/GenX_IR
rm -fv $RPM_BUILD_ROOT/usr/bin/iga64
rm -fv $RPM_BUILD_ROOT/usr/bin/clang-8
rm -fv $RPM_BUILD_ROOT/usr/include/opencl-c.h
chmod +x $RPM_BUILD_ROOT/usr/lib64/libopencl-clang.so.8

echo "==== DONE ===="

%files core
%defattr(-,root,root)
/usr/lib64/libiga64.so.%{major_version}
/usr/lib64/libiga64.so.%{major_version}.%{minor_version}.10
/usr/lib64/libigc.so.%{major_version}
/usr/lib64/libigc.so.%{major_version}.%{minor_version}.10

%files opencl
%defattr(-,root,root)
/usr/lib64/libigdfcl.so.%{major_version}
/usr/lib64/libigdfcl.so.%{major_version}.%{minor_version}.10
/usr/lib64/libopencl-clang.so.8

%files opencl-devel
%defattr(-,root,root)
/usr/include/igc/*
/usr/include/iga/*
/usr/include/visa/*
/usr/lib64/libiga64.so
/usr/lib64/libigc.so
/usr/lib64/libigdfcl.so
/usr/lib64/pkgconfig/*

%doc

%changelog
