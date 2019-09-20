%global neo_commit_id 5d640e7

Name: intel-opencl
Version: 19.37.14191
Release: 3%{?dist}
Summary: Intel(R) Graphics Compute Runtime for OpenCL(TM)

Group: System Environment/Libraries
License: MIT
URL: https://github.com/intel/compute-runtime
Source0: https://github.com/intel/compute-runtime/archive/%{neo_commit_id}/neo-%{neo_commit_id}.tar.gz

%if 0%{?el7}
BuildRequires: centos-release-scl epel-release
BuildRequires: devtoolset-7-gcc-c++ cmake3 make
%else
BuildRequires: make libva-devel gcc-c++ cmake
%endif

BuildRequires: intel-gmmlib-devel >= 19.2.3
BuildRequires: intel-igc-opencl-devel >= 1.0.2500

Requires: intel-gmmlib >= 19.2.3
Requires: intel-igc-opencl >= 1.0.2500

%description
Intel(R) Graphics Compute Runtime for OpenCL(TM).

%prep

%build
rm -rf *

mkdir neo
tar xzf $RPM_SOURCE_DIR/neo-%{neo_commit_id}.tar.gz -C neo --strip-components=1

mkdir build
cd build

%if 0%{?el7}
scl enable devtoolset-7 "cmake3 ../neo -DCMAKE_BUILD_TYPE=Release -DNEO_DRIVER_VERSION=%{version}"
scl enable devtoolset-7 "make -j`nproc` igdrcl_dll"
%else
cmake ../neo -DCMAKE_BUILD_TYPE=Release -DNEO_DRIVER_VERSION=%{version}
make -j`nproc` igdrcl_dll
%endif

%install
mkdir -p $RPM_BUILD_ROOT/usr/lib64/intel-opencl
mkdir -p $RPM_BUILD_ROOT/etc/OpenCL/vendors
cp $RPM_BUILD_DIR/build/bin/libigdrcl.so $RPM_BUILD_ROOT/usr/lib64/intel-opencl
strip $RPM_BUILD_ROOT/usr/lib64/intel-opencl/libigdrcl.so
echo "/usr/lib64/intel-opencl/libigdrcl.so" > $RPM_BUILD_ROOT/etc/OpenCL/vendors/intel.icd

%files
%defattr(-,root,root)
/usr/lib64/intel-opencl/libigdrcl.so

%config(noreplace)
/etc/OpenCL/vendors/intel.icd

# %doc

# %changelog
