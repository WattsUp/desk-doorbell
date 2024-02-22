# Desk doorbell

A attention-getting doorbell for coworkers to let me know they want something and I don't notice.

This repository is a personal project. But you may adapt it if you want.

# Requirements
### MacOS

```bash
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc cmake ninja
```

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential ninja-build
```

### Windows

[Windows Subsystem for Linux (WSL)](https://docs.microsoft.com/en-us/windows/wsl/install) can be used to run Ubuntu.

# Setup

Updates the [pico-sdk](https://github.com/raspberrypi/pico-sdk) sub-module, this may take a few minutes as its quite large.

```bash
git submodule update --init --recursive
```

# Build

Creates a new `build` directory and uses [ninja](https://ninja-build.org/) to build the .uf2 file.

```bash
mkdir build
cd build
cmake -G Ninja ..
ninja
```

Copy the .uf2 file from the `build` directory onto the pico to run

# Hardware
A pi pico with a NO button on pin 1. And two NEOPixel LED rings on pin 0. I have a 24 count on my monitor then a 12 count on the button.
