Summary:  A publish/subscribe event communication system
Name: echo
Version: 2.2
Release: 22
Group: Development/Libraries
Copyright: Reserved
Source: echo-2.2.tar.gz
URL: http://www.cc.gatech.edu/systems/projects/ECho
Packager: eisen@cc.gatech.edu
BuildRoot: %{_tmppath}/%{name}-%{version}-root
%description
pub/sub comm middleware
%define __check_files	%{nil}
%prep
%setup
%build
%configure
make
%makeinstall
if (test ! -r $RPM_BUILD_ROOT/usr/lib/cercs_config) ; then
    cp cercs_config $RPM_BUILD_ROOT/usr/lib/cercs_config
fi

%clean
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}

%post
echo in post install
if test X"$RPM_INSTALL_PREFIX" = X""; then
    RPM_INSTALL_PREFIX=/usr/local
    export RPM_INSTALL_PREFIX
    echo "Installed libraries in other than /usr/local/lib."
    echo "    set LD_LIBRARY_PATH to $RPM_INSTALL_PREFIX"
fi
for lib in libatl.la libcercs_env.la libcm.la libcmmulticast.la libcmrpc.la libcmrudp.la libcmselect.la libcmsockets.la libcommgrp.la libdrisc.la libecho.la libecl.la libIO.la libgen_thread.la; do
sed "s%/usr/local%$RPM_INSTALL_PREFIX%g" < $RPM_INSTALL_PREFIX/lib/$lib > /tmp/$lib
mv /tmp/$lib $RPM_INSTALL_PREFIX/lib/$lib
done
if ! grep "^$RPM_INSTALL_PREFIX/lib$" /etc/ld.so.conf 2>&1 >/dev/null
then
    echo "$RPM_INSTALL_PREFIX/lib" >> /etc/ld.so.conf
fi
/sbin/ldconfig -X

%postun
/sbin/ldconfig
%files
%defattr(-,root,root)
%config(noreplace) %{_libdir}/cercs_config
/


