Name: vdo
Version: 1
Release: 1%{?dist}
Summary: A set of userspace tools for managing pools deduplicated and/or compressed block storage.

License: GNU General Public License v2.0
URL: https://github.com/dm-vdo/vdo
Source0: https://github.com/dm-vdo/vdo/archive/master.zip

BuildRequires: gcc zlib-devel
Requires: kvdo

%description
VDO (which includes kvdo and vdo) is software that provides inline block-level deduplication, compression, and thin provisioning capabilities for primary storage. VDO installs within the Linux device mapper framework, where it takes ownership of existing physical block devices and remaps these to new, higher-level block devices with data-efficiency capabilities.

%prep
%setup -q -n %{name}-master

%build
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT

%make_install

%files
%license /COPYING
%doc /examples/ansible/README.txt
%doc /examples/ansible/test_vdocreate.yml
%doc /examples/ansible/test_vdocreate_alloptions.yml
%doc /examples/ansible/test_vdoremove.yml
%doc /examples/ansible/vdo.py
%doc /examples/ansible/vdo.pyc
%doc /examples/ansible/vdo.pyo
%doc /examples/nagios/nagios_check_albserver.pl
%doc /examples/nagios/nagios_check_vdostats_logicalSpace.pl
%doc /examples/nagios/nagios_check_vdostats_physicalSpace.pl
%doc /examples/nagios/nagios_check_vdostats_savingPercent.pl
%doc /examples/systemd/VDO.mount.example

/man8/vdo.8
/man8/vdodumpconfig.8
/man8/vdodumpmetadata.8
/man8/vdoforcerebuild.8
/man8/vdoformat.8
/man8/vdostats.8
/statistics/Command.py
/statistics/Command.pyc
/statistics/Command.pyo
/statistics/Field.py
/statistics/Field.pyc
/statistics/Field.pyo
/statistics/KernelStatistics.py
/statistics/KernelStatistics.pyc
/statistics/KernelStatistics.pyo
/statistics/LabeledValue.py
/statistics/LabeledValue.pyc
/statistics/LabeledValue.pyo
/statistics/StatFormatter.py
/statistics/StatFormatter.pyc
/statistics/StatFormatter.pyo
/statistics/StatStruct.py
/statistics/StatStruct.pyc
/statistics/StatStruct.pyo
/statistics/VDOReleaseVersions.py
/statistics/VDOReleaseVersions.pyc
/statistics/VDOReleaseVersions.pyo
/statistics/VDOStatistics.py
/statistics/VDOStatistics.pyc
/statistics/VDOStatistics.pyo
/statistics/__init__.py
/statistics/__init__.pyc
/statistics/__init__.pyo
/utils/Command.py
/utils/Command.pyc
/utils/Command.pyo
/utils/FileUtils.py
/utils/FileUtils.pyc
/utils/FileUtils.pyo
/utils/Logger.py
/utils/Logger.pyc
/utils/Logger.pyo
/utils/Timeout.py
/utils/Timeout.pyc
/utils/Timeout.pyo
/utils/Transaction.py
/utils/Transaction.pyc
/utils/Transaction.pyo
/utils/YAMLObject.py
/utils/YAMLObject.pyc
/utils/YAMLObject.pyo
/utils/__init__.py
/utils/__init__.pyc
/utils/__init__.pyo
/vdo
/vdo.service
/vdodumpconfig
/vdoforcerebuild
/vdoformat
/vdomgmnt/CommandLock.py
/vdomgmnt/CommandLock.pyc
/vdomgmnt/CommandLock.pyo
/vdomgmnt/Configuration.py
/vdomgmnt/Configuration.pyc
/vdomgmnt/Configuration.pyo
/vdomgmnt/Constants.py
/vdomgmnt/Constants.pyc
/vdomgmnt/Constants.pyo
/vdomgmnt/Defaults.py
/vdomgmnt/Defaults.pyc
/vdomgmnt/Defaults.pyo
/vdomgmnt/KernelModuleService.py
/vdomgmnt/KernelModuleService.pyc
/vdomgmnt/KernelModuleService.pyo
/vdomgmnt/MgmntLogger.py
/vdomgmnt/MgmntLogger.pyc
/vdomgmnt/MgmntLogger.pyo
/vdomgmnt/MgmntUtils.py
/vdomgmnt/MgmntUtils.pyc
/vdomgmnt/MgmntUtils.pyo
/vdomgmnt/Service.py
/vdomgmnt/Service.pyc
/vdomgmnt/Service.pyo
/vdomgmnt/SizeString.py
/vdomgmnt/SizeString.pyc
/vdomgmnt/SizeString.pyo
/vdomgmnt/Utils.py
/vdomgmnt/Utils.pyc
/vdomgmnt/Utils.pyo
/vdomgmnt/VDOKernelModuleService.py
/vdomgmnt/VDOKernelModuleService.pyc
/vdomgmnt/VDOKernelModuleService.pyo
/vdomgmnt/VDOOperation.py
/vdomgmnt/VDOOperation.pyc
/vdomgmnt/VDOOperation.pyo
/vdomgmnt/VDOService.py
/vdomgmnt/VDOService.pyc
/vdomgmnt/VDOService.pyo
/vdomgmnt/__init__.py
/vdomgmnt/__init__.pyc
/vdomgmnt/__init__.pyo
/vdoprepareupgrade
/vdoreadonly
/vdostats

%changelog
* Tue Oct 31 2017 Robert de Bock <robert@meinit.nl>
- Initial build.
