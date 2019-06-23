# `libhantek6xx4`: Open Source Hantek 6xx4-series USB Oscilloscope Support

This library is intended to provide an abstraction on top of the Hantek 6xx4 series oscilloscopes, using `libusb-1.0`.

## Dependencies
You'll need to install the following:
 * `libusb-1.0` - to access the device using libusb
 * `pkg-config` - for build-time configuration
 * `gcc`, `ld`, etc. - for actually compiling the damned thing

## Building
If you have all the dependencies available, simply invoking `make` should do the job
