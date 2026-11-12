Name:           miryu-hardware-manager
Version:        45.0.0
Release:        4%{?dist}
Summary:        Hardware information and driver manager for Fedora

License:        GPL-3.0-or-later
URL:            https://github.com/evernightvista/miryu-hardware-manager
Source0:        https://github.com/evernightvista/miryu-hardware-manager/archive/refs/heads/main.zip

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-qtbase-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kcrash-devel
BuildRequires:  desktop-file-utils
BuildRequires:  gettext

Requires:       pciutils
Requires:       dmidecode
Requires:       rpm
Requires:       mokutil
Requires:       polkit
Requires:       dnf
Requires:       hicolor-icon-theme
Obsoletes:      nvidia-driver-manager
Obsoletes:      nvidia-driver-manager-selinux

%description
Miryu Hardware Manager is a Qt 6 / KDE Frameworks 6 application for Fedora.
It shows the operating system, processor, memory, storage and graphics card
information and manages device drivers.

The driver manager can install or switch NVIDIA driver branches, configure
Secure Boot MOK enrolment, revert to the Nouveau driver and install or remove
the Logitech TrueForce and Razer OpenRazer kernel drivers. Every privileged
operation runs through polkit and displays a live installation log.

%prep
%autosetup -n %{name}-main

%build
%cmake
%cmake_build

%install
%cmake_install
%find_lang %{name}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

%files -f %{name}.lang
%license LICENSE
%{_bindir}/%{name}
%{_libexecdir}/miryu-hardware-manager-helper
%{_libexecdir}/miryu-hardware-manager-mok-helper
%{_libexecdir}/miryu-hardware-manager-mok-cert-helper
%{_libexecdir}/miryu-hardware-manager-dmi-helper
%{_datadir}/applications/%{name}.desktop
%{_datadir}/polkit-1/actions/org.miryugaming.hardwaremanager.policy
%{_datadir}/polkit-1/actions/org.miryugaming.hardwaremanager.dmi.policy

%changelog
* Fri Nov 12 2026 KairikiFedora <13278297951@sina.cn> - 45.0.0-4
- Fix no event sound bug

* Wed Nov 11 2026 KairikiFedora <13278297951@sina.cn> - 45.0.0-3
- Fix BUG

* Wed Nov 11 2026 KairikiFedora <13278297951@sina.cn> - 45.0.0-2
- Fix Polkit Vendor Link

* Wed Nov 11 2026 KairikiFedora <13278297951@sina.cn> - 45.0.0-1
- Initial Release
