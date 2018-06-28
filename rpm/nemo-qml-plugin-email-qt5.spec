Name:       nemo-qml-plugin-email-qt5
Summary:    Email plugin for Nemo Mobile
Version:    0.3.2
Release:    1
Group:      System/Libraries
License:    BSD
URL:        https://git.merproject.org/mer-core/nemo-qml-plugin-email
Source0:    %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(mlocale5)
BuildRequires:  pkgconfig(qmfmessageserver5)
BuildRequires:  pkgconfig(qmfclient5)
Requires:  libqmfmessageserver1-qt5 >= 4.0.4+git50
Requires:  libqmfclient1-qt5 >= 4.0.4+git50
Conflicts: nemo-qml-plugin-email-qt5-offline

%description
%{summary}.

%package devel
Summary:    Nemo email plugin support for C++ applications
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.

%package tests
Summary:    QML email plugin tests
Group:      System/Libraries
Requires:   qmf-qt5-tests >= 4.0.4+git16
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

# org.nemomobile.email legacy import
mkdir -p %{buildroot}%{_libdir}/qt5/qml/org/nemomobile/email/
ln -sf %{_libdir}/qt5/qml/Nemo/Email/libnemoemail.so %{buildroot}%{_libdir}/qt5/qml/org/nemomobile/email/
sed 's/Nemo.Email/org.nemomobile.email/' < src/plugin/qmldir > %{buildroot}%{_libdir}/qt5/qml/org/nemomobile/email/qmldir


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libnemoemail-qt5.so.*
%dir %{_libdir}/qt5/qml/Nemo/Email
%{_libdir}/qt5/qml/Nemo/Email/libnemoemail.so
%{_libdir}/qt5/qml/Nemo/Email/plugins.qmltypes
%{_libdir}/qt5/qml/Nemo/Email/qmldir
%{_sysconfdir}/xdg/nemo-qml-plugin-email/domainSettings.conf
%{_sysconfdir}/xdg/nemo-qml-plugin-email/serviceSettings.conf

# org.nemomobile.email legacy import
%dir %{_libdir}/qt5/qml/org/nemomobile/email
%{_libdir}/qt5/qml/org/nemomobile/email/libnemoemail.so
%{_libdir}/qt5/qml/org/nemomobile/email/qmldir

%files devel
%defattr(-,root,root,-)
%{_libdir}/libnemoemail-qt5.so
%{_libdir}/libnemoemail-qt5.prl
%{_includedir}/nemoemail-qt5/*.h
%{_libdir}/pkgconfig/nemoemail-qt5.pc

%files tests
%defattr(-,root,root,-)
/opt/tests/nemo-qml-plugins/email/*
