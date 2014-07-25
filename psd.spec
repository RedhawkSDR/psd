# This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this 
# source distribution.
#
# This file is part of REDHAWK Basic Components psd.
# 
# REDHAWK Basic Components psd is free software: you can redistribute it and/or modify it under the terms of 
# the GNU General Public License as published by the Free Software Foundation, either 
# version 3 of the License, or (at your option) any later version.
# 
# REDHAWK Basic Components psd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
# PURPOSE.  See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with this 
# program.  If not, see http://www.gnu.org/licenses/.
#

# By default, the RPM will install to the standard REDHAWK SDR root location (/var/redhawk/sdr)
# You can override this at install time using --prefix /new/sdr/root when invoking rpm (preferred method, if you must)
%{!?_sdrroot: %define _sdrroot /var/redhawk/sdr}
%define _prefix %{_sdrroot}
Prefix:         %{_prefix}

# Point install paths to locations within our target SDR root
%define _sysconfdir    %{_prefix}/etc
%define _localstatedir %{_prefix}/var
%define _mandir        %{_prefix}/man
%define _infodir       %{_prefix}/info

Name:           psd
Version:        1.0.0
Release:        9%{?dist}
Summary:        Component %{name}

Group:          REDHAWK/Components
License:        GPLv3+
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  redhawk-devel >= 1.10
Requires:       redhawk >= 1.10

# Interface requirements
BuildRequires:  bulkioInterfaces
Requires:       bulkioInterfaces

# Softpkg requirements
BuildRequires:  dsp-devel >= 1.0.0
Requires:       dsp >= 1.0.0
BuildRequires:  fftlib-devel >= 1.0.0
Requires:       fftlib >= 1.0.0

%description
Component %{name}
 * Commit: __REVISION__
 * Source Date/Time: __DATETIME__


%prep
%setup -q


%build
# Implementation cpp
pushd cpp
./reconf
%define _bindir %{_prefix}/dom/components/psd/cpp
%configure
make %{?_smp_mflags}
popd


%install
rm -rf $RPM_BUILD_ROOT
# Implementation cpp
pushd cpp
%define _bindir %{_prefix}/dom/components/psd/cpp
make install DESTDIR=$RPM_BUILD_ROOT
popd


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,redhawk,redhawk,-)
%dir %{_prefix}/dom/components/%{name}
%{_prefix}/dom/components/%{name}/psd.scd.xml
%{_prefix}/dom/components/%{name}/psd.prf.xml
%{_prefix}/dom/components/%{name}/psd.spd.xml
%{_prefix}/dom/components/%{name}/cpp

