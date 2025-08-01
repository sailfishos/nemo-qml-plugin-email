Name:       nemo-qml-plugin-email-qt5
Summary:    Email plugin for Nemo Mobile
Version:    0.6.36
Release:    1
License:    ASL 2.0 and LGPLv2+ and BSD
URL:        https://github.com/sailfishos/nemo-qml-plugin-email
Source0:    %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(QmfMessageServer) >= 4.0.4+git127
BuildRequires:  pkgconfig(QmfClient) >= 4.0.4+git165
BuildRequires:  pkgconfig(accounts-qt5)

%description
%{summary}.

%package offline
Summary:    Nemo email plugin configuration for forcing offline mode
Requires:   %{name} = %{version}-%{release}

%description offline
Installing this package forces the email agent to work in offline mode.
Mostly for demo purposes.

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
%qmake5 "VERSION=%{version}"
%make_build

%install
%qmake5_install

# org.nemomobile.email legacy import
mkdir -p %{buildroot}%{_libdir}/qt5/qml/org/nemomobile/email/
ln -sf %{_libdir}/qt5/qml/Nemo/Email/libnemoemail.so %{buildroot}%{_libdir}/qt5/qml/org/nemomobile/email/
sed 's/Nemo.Email/org.nemomobile.email/' < src/plugin/qmldir > %{buildroot}%{_libdir}/qt5/qml/org/nemomobile/email/qmldir

mkdir -p %{buildroot}/usr/lib/nemo-email
touch %{buildroot}/usr/lib/nemo-email/force_offline

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license LICENSE.ASL2 LICENSE.LGPL LICENSE.BSD
%{_libdir}/libnemoemail-qt5.so.*
%dir %{_libdir}/qt5/qml/Nemo/Email
%{_libdir}/qt5/qml/Nemo/Email/libnemoemail.so
%{_libdir}/qt5/qml/Nemo/Email/plugins.qmltypes
%{_libdir}/qt5/qml/Nemo/Email/qmldir
%{_libdir}/qt5/plugins/messageserverplugins/libattachmentdownloader.so
%{_sysconfdir}/xdg/nemo-qml-plugin-email/domainSettings.conf
%{_sysconfdir}/xdg/nemo-qml-plugin-email/serviceSettings.conf

# org.nemomobile.email legacy import
%dir %{_libdir}/qt5/qml/org/nemomobile/email
%{_libdir}/qt5/qml/org/nemomobile/email/libnemoemail.so
%{_libdir}/qt5/qml/org/nemomobile/email/qmldir

%files offline
/usr/lib/nemo-email/force_offline

%files devel
%{_libdir}/libnemoemail-qt5.so
%{_libdir}/libnemoemail-qt5.prl
%{_includedir}/nemoemail-qt5/*.h
%{_libdir}/pkgconfig/nemoemail-qt5.pc

%files tests
/opt/tests/nemo-qml-plugin-email-qt5/*
