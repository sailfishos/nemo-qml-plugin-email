Name:       nemo-qml-plugin-email-qt6
Summary:    Email plugin for Nemo Mobile
Version:    0.6.68
Release:    1
License:    ASL 2.0 and LGPLv2+ and BSD
URL:        https://github.com/sailfishos/nemo-qml-plugin-email
Source0:    %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(Qt6Core)
BuildRequires:  pkgconfig(Qt6Gui)
BuildRequires:  pkgconfig(Qt6DBus)
BuildRequires:  pkgconfig(Qt6Qml)
BuildRequires:  pkgconfig(Qt6Network)
BuildRequires:  pkgconfig(Qt6Concurrent)
BuildRequires:  pkgconfig(Qt6Test)
BuildRequires:  pkgconfig(QmfMessageServer) >= 4.0.4+git127
BuildRequires:  pkgconfig(QmfClient) >= 4.0.4+git166
BuildRequires:  pkgconfig(accounts-qt6)

%description
%{summary}.

%package devel
Summary:    Nemo email plugin support for C++ applications
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.

%package tests
Summary:    QML email plugin tests
Requires:   %{name} = %{version}-%{release}
Requires:   blts-tools

%description tests
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build

export PATH=$PATH:%{_qt6_bindir}
%qmake_qt6 "VERSION=%{version}"
%make_build

%install
%make_install

# org.nemomobile.email legacy import
mkdir -p %{buildroot}%{_libdir}/qt6/qml/org/nemomobile/email/
ln -sf %{_libdir}/qt6/qml/Nemo/Email/libnemoemail.so %{buildroot}%{_libdir}/qt6/qml/org/nemomobile/email/
sed 's/Nemo.Email/org.nemomobile.email/' < src/plugin/qmldir > %{buildroot}%{_libdir}/qt6/qml/org/nemomobile/email/qmldir

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license LICENSE.ASL2 LICENSE.LGPL LICENSE.BSD
%{_libdir}/libnemoemail-qt6.so.*
%dir %{_libdir}/qt6/qml/Nemo/Email
%{_libdir}/qt6/qml/Nemo/Email/libnemoemail.so
%{_libdir}/qt6/qml/Nemo/Email/plugins.qmltypes
%{_libdir}/qt6/qml/Nemo/Email/qmldir
%{_libdir}/qt6/plugins/messagingframework/messageserverplugins/libattachmentdownloader.so
%{_sysconfdir}/xdg/nemo-qml-plugin-email/domainSettings.conf
%{_sysconfdir}/xdg/nemo-qml-plugin-email/serviceSettings.conf

# org.nemomobile.email legacy import
%dir %{_libdir}/qt6/qml/org/nemomobile/email
%{_libdir}/qt6/qml/org/nemomobile/email/libnemoemail.so
%{_libdir}/qt6/qml/org/nemomobile/email/qmldir

%files devel
%{_libdir}/libnemoemail-qt6.so
%{_libdir}/libnemoemail-qt6.prl
%{_includedir}/nemoemail-qt6/*.h
%{_libdir}/pkgconfig/nemoemail-qt6.pc

%files tests
/opt/tests/nemo-qml-plugin-email-qt6/*
