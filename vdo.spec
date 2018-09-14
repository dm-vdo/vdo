%define spec_release 1
#
#
#
Summary: Management tools for Virtual Data Optimizer
Name: vdo
Version: 6.1.1.125
Release: %{spec_release}
License: GPLv2
Source: %{name}-%{version}.tgz
URL: http://github.com/dm-vdo/vdo
Distribution: RHEL 7.3
Requires: PyYAML >= 3.10
Requires: libuuid >= 2.23
Requires: kmod-kvdo >= 6.1
Requires: lvm2 >= 2.02.171
Provides: kvdo-kmod-common = %{version}
ExclusiveArch: x86_64
ExcludeArch: s390
ExcludeArch: s390x
ExcludeArch: ppc
ExcludeArch: ppc64
ExcludeArch: ppc64le
ExcludeArch: aarch64
ExcludeArch: i686
BuildRequires: device-mapper-event-devel
BuildRequires: gcc
BuildRequires: libuuid-devel
BuildRequires: python
BuildRequires: python-devel
BuildRequires: systemd
BuildRequires: valgrind-devel
BuildRequires: zlib-devel
%{?systemd_requires}

# Disable an automatic dependency due to a file in examples/monitor.
%define __requires_exclude perl

%description
Virtual Data Optimizer (VDO) is a device mapper target that delivers
block-level deduplication, compression, and thin provisioning.

This package provides the user-space management tools for VDO.

%prep
%setup -q

%build
make

%install
make install DESTDIR=$RPM_BUILD_ROOT INSTALLOWNER= bindir=%{_bindir} \
  defaultdocdir=%{_defaultdocdir} name=%{name} \
  python_sitelib=%{python_sitelib} mandir=%{_mandir} \
  unitdir=%{_unitdir} presetdir=%{_presetdir}

%post
%systemd_post vdo.service

%preun
%systemd_preun vdo.service

%postun
%systemd_postun_with_restart vdo.service

%files
#defattr(-,root,root)
%{_bindir}/vdo
%{_bindir}/vdostats
%{_bindir}/vdodmeventd
%{_bindir}/vdodumpconfig
%{_bindir}/vdoforcerebuild
%{_bindir}/vdoformat
%{_bindir}/vdoprepareupgrade
%{_bindir}/vdoreadonly
%dir %{python_sitelib}/%{name}
%dir %{python_sitelib}/%{name}/vdomgmnt/
%{python_sitelib}/%{name}/vdomgmnt/CommandLock.py
%{python_sitelib}/%{name}/vdomgmnt/CommandLock.pyc
%{python_sitelib}/%{name}/vdomgmnt/CommandLock.pyo
%{python_sitelib}/%{name}/vdomgmnt/Configuration.py
%{python_sitelib}/%{name}/vdomgmnt/Configuration.pyc
%{python_sitelib}/%{name}/vdomgmnt/Configuration.pyo
%{python_sitelib}/%{name}/vdomgmnt/Constants.py
%{python_sitelib}/%{name}/vdomgmnt/Constants.pyc
%{python_sitelib}/%{name}/vdomgmnt/Constants.pyo
%{python_sitelib}/%{name}/vdomgmnt/Defaults.py
%{python_sitelib}/%{name}/vdomgmnt/Defaults.pyc
%{python_sitelib}/%{name}/vdomgmnt/Defaults.pyo
%{python_sitelib}/%{name}/vdomgmnt/ExitStatusMixins.py
%{python_sitelib}/%{name}/vdomgmnt/ExitStatusMixins.pyc
%{python_sitelib}/%{name}/vdomgmnt/ExitStatusMixins.pyo
%{python_sitelib}/%{name}/vdomgmnt/KernelModuleService.py
%{python_sitelib}/%{name}/vdomgmnt/KernelModuleService.pyc
%{python_sitelib}/%{name}/vdomgmnt/KernelModuleService.pyo
%{python_sitelib}/%{name}/vdomgmnt/MgmntLogger.py
%{python_sitelib}/%{name}/vdomgmnt/MgmntLogger.pyc
%{python_sitelib}/%{name}/vdomgmnt/MgmntLogger.pyo
%{python_sitelib}/%{name}/vdomgmnt/MgmntUtils.py
%{python_sitelib}/%{name}/vdomgmnt/MgmntUtils.pyc
%{python_sitelib}/%{name}/vdomgmnt/MgmntUtils.pyo
%{python_sitelib}/%{name}/vdomgmnt/Service.py
%{python_sitelib}/%{name}/vdomgmnt/Service.pyc
%{python_sitelib}/%{name}/vdomgmnt/Service.pyo
%{python_sitelib}/%{name}/vdomgmnt/SizeString.py
%{python_sitelib}/%{name}/vdomgmnt/SizeString.pyc
%{python_sitelib}/%{name}/vdomgmnt/SizeString.pyo
%{python_sitelib}/%{name}/vdomgmnt/Utils.py
%{python_sitelib}/%{name}/vdomgmnt/Utils.pyc
%{python_sitelib}/%{name}/vdomgmnt/Utils.pyo
%{python_sitelib}/%{name}/vdomgmnt/VDOService.py
%{python_sitelib}/%{name}/vdomgmnt/VDOService.pyc
%{python_sitelib}/%{name}/vdomgmnt/VDOService.pyo
%{python_sitelib}/%{name}/vdomgmnt/VDOKernelModuleService.py
%{python_sitelib}/%{name}/vdomgmnt/VDOKernelModuleService.pyc
%{python_sitelib}/%{name}/vdomgmnt/VDOKernelModuleService.pyo
%{python_sitelib}/%{name}/vdomgmnt/VDOOperation.py
%{python_sitelib}/%{name}/vdomgmnt/VDOOperation.pyc
%{python_sitelib}/%{name}/vdomgmnt/VDOOperation.pyo
%{python_sitelib}/%{name}/vdomgmnt/__init__.py
%{python_sitelib}/%{name}/vdomgmnt/__init__.pyc
%{python_sitelib}/%{name}/vdomgmnt/__init__.pyo
%dir %{python_sitelib}/%{name}/statistics/
%{python_sitelib}/%{name}/statistics/Command.py
%{python_sitelib}/%{name}/statistics/Command.pyc
%{python_sitelib}/%{name}/statistics/Command.pyo
%{python_sitelib}/%{name}/statistics/Field.py
%{python_sitelib}/%{name}/statistics/Field.pyc
%{python_sitelib}/%{name}/statistics/Field.pyo
%{python_sitelib}/%{name}/statistics/KernelStatistics.py
%{python_sitelib}/%{name}/statistics/KernelStatistics.pyc
%{python_sitelib}/%{name}/statistics/KernelStatistics.pyo
%{python_sitelib}/%{name}/statistics/LabeledValue.py
%{python_sitelib}/%{name}/statistics/LabeledValue.pyc
%{python_sitelib}/%{name}/statistics/LabeledValue.pyo
%{python_sitelib}/%{name}/statistics/StatFormatter.py
%{python_sitelib}/%{name}/statistics/StatFormatter.pyc
%{python_sitelib}/%{name}/statistics/StatFormatter.pyo
%{python_sitelib}/%{name}/statistics/StatStruct.py
%{python_sitelib}/%{name}/statistics/StatStruct.pyc
%{python_sitelib}/%{name}/statistics/StatStruct.pyo
%{python_sitelib}/%{name}/statistics/VDOReleaseVersions.py
%{python_sitelib}/%{name}/statistics/VDOReleaseVersions.pyc
%{python_sitelib}/%{name}/statistics/VDOReleaseVersions.pyo
%{python_sitelib}/%{name}/statistics/VDOStatistics.py
%{python_sitelib}/%{name}/statistics/VDOStatistics.pyc
%{python_sitelib}/%{name}/statistics/VDOStatistics.pyo
%{python_sitelib}/%{name}/statistics/__init__.py
%{python_sitelib}/%{name}/statistics/__init__.pyc
%{python_sitelib}/%{name}/statistics/__init__.pyo
%dir %{python_sitelib}/%{name}/utils/
%{python_sitelib}/%{name}/utils/Command.py
%{python_sitelib}/%{name}/utils/Command.pyc
%{python_sitelib}/%{name}/utils/Command.pyo
%{python_sitelib}/%{name}/utils/FileUtils.py
%{python_sitelib}/%{name}/utils/FileUtils.pyc
%{python_sitelib}/%{name}/utils/FileUtils.pyo
%{python_sitelib}/%{name}/utils/Logger.py
%{python_sitelib}/%{name}/utils/Logger.pyc
%{python_sitelib}/%{name}/utils/Logger.pyo
%{python_sitelib}/%{name}/utils/Timeout.py
%{python_sitelib}/%{name}/utils/Timeout.pyc
%{python_sitelib}/%{name}/utils/Timeout.pyo
%{python_sitelib}/%{name}/utils/Transaction.py
%{python_sitelib}/%{name}/utils/Transaction.pyc
%{python_sitelib}/%{name}/utils/Transaction.pyo
%{python_sitelib}/%{name}/utils/YAMLObject.py
%{python_sitelib}/%{name}/utils/YAMLObject.pyc
%{python_sitelib}/%{name}/utils/YAMLObject.pyo
%{python_sitelib}/%{name}/utils/__init__.py
%{python_sitelib}/%{name}/utils/__init__.pyc
%{python_sitelib}/%{name}/utils/__init__.pyo
%{_unitdir}/vdo.service
%{_presetdir}/97-vdo.preset
%dir %{_defaultdocdir}/%{name}
%license %{_defaultdocdir}/%{name}/COPYING
%dir %{_defaultdocdir}/%{name}/examples
%dir %{_defaultdocdir}/%{name}/examples/ansible
%doc %{_defaultdocdir}/%{name}/examples/ansible/README.txt
%doc %{_defaultdocdir}/%{name}/examples/ansible/test_vdocreate.yml
%doc %{_defaultdocdir}/%{name}/examples/ansible/test_vdocreate_alloptions.yml
%doc %{_defaultdocdir}/%{name}/examples/ansible/test_vdoremove.yml
%doc %{_defaultdocdir}/%{name}/examples/ansible/vdo.py
# Fedora doesn't byte-compile the examples.
%if 0%{?rhel}
%doc %{_defaultdocdir}/%{name}/examples/ansible/vdo.pyc
%doc %{_defaultdocdir}/%{name}/examples/ansible/vdo.pyo
%endif
%dir %{_defaultdocdir}/%{name}/examples/monitor
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_logicalSpace.pl
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_physicalSpace.pl
%doc %{_defaultdocdir}/%{name}/examples/monitor/monitor_check_vdostats_savingPercent.pl
%dir %{_defaultdocdir}/%{name}/examples/systemd
%doc %{_defaultdocdir}/%{name}/examples/systemd/VDO.mount.example
%{_mandir}/man8/vdo.8.gz
%{_mandir}/man8/vdostats.8.gz
%{_mandir}/man8/vdodmeventd.8.gz
%{_mandir}/man8/vdodumpconfig.8.gz
%{_mandir}/man8/vdodumpmetadata.8.gz
%{_mandir}/man8/vdoforcerebuild.8.gz
%{_mandir}/man8/vdoformat.8.gz


%changelog
* Fri Sep 14 2018 - J. corwin Coburn <corwin@redhat.com> - 6.1.1.125-1
HASH(0x17dbb28)