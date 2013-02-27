#
#	RPM spec file for the Embedthis GoAhead HTTP web server
#
Summary: ${settings.title} -- Embeddable HTTP Web Server
Name: ${settings.product}
Version: ${settings.version}
Release: ${settings.buildNumber}
License: Dual Open Source/commercial
Group: Applications/Internet
URL: http://embedthis.com
Distribution: Embedthis
Vendor: Embedthis Software
BuildRoot: ${prefixes.rpm}/BUILDROOT/${settings.product}-${settings.version}-${settings.buildNumber}.${platform.mappedCpu}
AutoReqProv: no

%description
Embedthis GoAhead is a fast, compact and simple to use embedded web server.

%prep

%build

%install
    if [ -x /usr/lib/goahead/latest/bin/uninstall ] ; then
        goahead_HEADLESS=1 /usr/lib/goahead/latest/bin/uninstall </dev/null 2>&1 >/dev/null
    fi
    mkdir -p ${prefixes.rpm}/BUILDROOT/${settings.product}-${settings.version}-${settings.buildNumber}.${platform.mappedCpu}
    cp -r ${prefixes.content}/* ${prefixes.rpm}/BUILDROOT/${settings.product}-${settings.version}-${settings.buildNumber}.${platform.mappedCpu}

%clean

%files -f binFiles.txt

%post
if [ -x /usr/bin/chcon ] ; then 
	sestatus | grep enabled >/dev/null 2>&1
	if [ $? = 0 ] ; then
		for f in ${prefixes.vapp}/bin/*.so ; do
			chcon /usr/bin/chcon -t texrel_shlib_t $f
		done
	fi
fi
ldconfig -n ${prefixes.vapp}/bin

%preun
rm -f ${prefixes.app}/latest

%postun
