# rtl\_tools

Standalone versions of `rtl_fm`, `rtl_power`, and `rtl_sdr` based on [librtlsdr](https://github.com/librtlsdr/librtlsdr),
but using the [SoapySDR](https://github.com/pothosware/SoapySDR) vendor-neutral SDR support library instead, intended
to support a wider range of devices than RTL-SDR.

## Installation

[Install SoapySDR](https://github.com/pothosware/SoapySDR/wiki#installation), then run:

    make

## Tools included

After building, these binaries should then be available at the root directory:

* `rtl_fm`: demodulator for FM and other modes, see [rtl\_fm guide](http://kmkeen.com/rtl-demod-guide/index.html)

* `rtl_power`: FFT logger, see [rtl\_power](http://kmkeen.com/rtl-power/)

* `rtl_sdr`: emits raw I/Q data

### Not included

Tools from librtlsdr not included in this repository:

* `rtl_eeprom`, `rtl_test`, `rtl_ir`: specific to RTL-SDR devices
* `rtl_tcp`, `rtl_rpcd`: remote networking, see [SoapyRemote](https://github.com/pothosware/SoapyRemote) instead
* `rtl_adsb`: see [dump1090](https://github.com/mutability/dump1090)

## Device support

Currently primilarly tested with an RTL-SDR dongle, but supporting other devices
supported by SoapySDR is the goal. Experimental, use at your own risk, but bug
reports and patches are welcome.

