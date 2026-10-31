LeapC.h, libLeapC and the Ultraleap tracking service are external SDK
components governed by the Ultraleap SDK agreement. They are not distributed
in this repository. Install the SDK before building; CMake searches for its
LeapC.h header. The wrapper source is Apache-2.0 licensed.

scripts/install_service.sh downloads the archived official Gemini 5.17.1.0
Debian package, verifies its recorded SHA-256, and installs it locally.
Consult the SDK agreement supplied with that package for its terms.

SDK documentation: https://docs.ultraleap.com/linux/index.html
