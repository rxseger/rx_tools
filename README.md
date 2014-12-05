# librtlsdr Raspian packages

This branch provides Raspbian-flavoured Debian packages for librtlsdr.
They are based on the packaging done for the Debian wheezy-backports package.

The changes are:

 * These packages are built for the armhf architecture as used by Raspbian, rather than the Debian standard armhf which is unfortunately incompatible with Raspbian.
 * The package is based on the current git master version of rtl-sdr.
