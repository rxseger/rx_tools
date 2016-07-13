# rtl\_tools

Standalone versions of `rtl_fm`, `rtl_power`, and `rtl_sdr` based on [librtlsdr](https://github.com/librtlsdr/librtlsdr)

Tested with [rxseger/librtlsdr](https://github.com/rxseger/librtlsdr), to install on Mac OS X with [Homebrew](http://brew.sh):

    brew tap rxseger/hackrf
    brew install --HEAD rxseger/hackrf/librtlsdr

then to build `rtl_tools`:

    make

These binaries should then be available at the root directory:

* `rtl_fm`: demodulator for FM and other modes, see [rtl\_fm guide](http://kmkeen.com/rtl-demod-guide/index.html)

* `rtl_power`: FFT logger, see [rtl\_power](http://kmkeen.com/rtl-power/)

* `rtl_sdr`: emits raw I/Q data

Why build `rtl_tools` from this repository instead of along with librtlsdr?
Mainly for easier development, no need to rebuild or fork the entire library
just to make a change to one of these tools.


Tools from librtlsdr not included in this repository:

* `rtl_eeprom`, `rtl_test`, `rtl_ir`: specific to RTL-SDR devices
* `rtl_tcp`, `rtl_rpcd`: remote networking
* `rtl_adsb`: see [dump1090](https://github.com/mutability/dump1090)


## For more information see:

https://github.com/librtlsdr/librtlsdr
