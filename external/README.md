# External Dependencies
This directory contains all the external dependencies used by the project and is currently only needed when building on 64-bit Windows.

### Download-Deps.ps1
This script will download the SDL2 development packages needed to build and run msemu on Windows and prepare the external include/lib dirs.

### z80ex
On Windows, this dependency must be built manually as there is not a pre-built version available. This is done via CMake's `ExternalProject` feature, which clones the z80ex repo, applies patches (see below), and builds the library.

#### Patches
* `z80ex.0001.patch` - fixes a compilation error in z80ex w/ msvc