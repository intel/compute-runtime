%global neo_commit_id 874ae355f76e93c69ea626aad7e86a41756529aa

Name: intel-opencl
Version: 19.32.13826
Release: 1%{?dist}
Summary: Intel(R) Graphics Compute Runtime for OpenCL(TM)

Group: System Environment/Libraries
License: MIT
URL: https://github.com/intel/compute-runtime
Source0: https://github.com/intel/compute-runtime/archive/%{neo_commit_id}/neo-%{neo_commit_id}.tar.gz

%if 0%{?el7}
BuildRequires: centos-release-scl epel-release
BuildRequires: devtoolset-4-gcc-c++ cmake3 make
%else
BuildRequires: make libva-devel gcc-c++ cmake
%endif

BuildRequires: intel-gmmlib-devel >= 19.2.3
BuildRequires: intel-igc-opencl-devel >= 1.0.10

Requires: intel-gmmlib >= 19.2.3
Requires: intel-igc-opencl >= 1.0.10

%description
Intel(R) Graphics Compute Runtime for OpenCL(TM).

%prep


%build
echo "==== BUILD ===="
rm -rf *

mkdir neo
tar xzf $RPM_SOURCE_DIR/neo-%{neo_commit_id}.tar.gz -C neo --strip-components=1

mkdir build
cd build

%if 0%{?el7}
scl enable devtoolset-4 "cmake3 ../neo -DCMAKE_BUILD_TYPE=Release -DNEO_DRIVER_VERSION=%{version}"
scl enable devtoolset-4 "make -j`nproc` igdrcl_dll"
%else
cmake ../neo -DCMAKE_BUILD_TYPE=Release -DNEO_DRIVER_VERSION=%{version}
make -j`nproc` igdrcl_dll
%endif

echo "==== DONE ===="


%install
echo "==== INSTALL ===="
mkdir -p $RPM_BUILD_ROOT/usr/lib64/intel-opencl
mkdir -p $RPM_BUILD_ROOT/etc/OpenCL/vendors
cp $RPM_BUILD_DIR/build/bin/libigdrcl.so $RPM_BUILD_ROOT/usr/lib64/intel-opencl
strip $RPM_BUILD_ROOT/usr/lib64/intel-opencl/libigdrcl.so
echo "/usr/lib64/intel-opencl/libigdrcl.so" > $RPM_BUILD_ROOT/etc/OpenCL/vendors/intel.icd
echo "==== DONE ===="


%files
%defattr(-,root,root)
/usr/lib64/intel-opencl/libigdrcl.so

%config(noreplace)
/etc/OpenCL/vendors/intel.icd

# %doc


# %changelog
