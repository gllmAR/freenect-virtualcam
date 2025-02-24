# freenect-virtualcam

freenectVirtualCamera is a cross-platform program that captures frames from a Kinect sensor using libfreenect and forwards them to a virtual video device. On Linux, it writes frames to a v4l2loopback device, allowing any V4L2-compliant application to access the Kinect stream.

## Features

- **IR/RGB Streaming:** Choose between infrared (IR) or RGB video streaming (only one can be enabled at a time).
- **Depth Streaming:** Optionally enable depth streaming (converted to 8-bit grayscale).
- **Auto-Reconnection:** Automatically reconnects if the Kinect sensor disconnects.
- **Configurable Loopback Device:** Specify which v4l2loopback device to use via a command-line parameter.

## Requirements

- **Linux** (fully supported; macOS and Windows virtual device support are not implemented)
- [libfreenect](https://github.com/OpenKinect/libfreenect)
- v4l2loopback kernel module (for virtual camera support)
- C++11 compatible compiler
- CMake (version 3.10 or later)

## Installation

1. **Install Dependencies (Debian/Ubuntu):**
   ```bash
   sudo apt-get update
   sudo apt-get install libfreenect-dev pkg-config cmake build-essential curl
   ```

2. **Install v4l2loopback:**
   ```bash
   sudo apt-get install v4l2loopback-dkms
   sudo modprobe v4l2loopback
   ```
   This will create one or more virtual video devices (e.g., `/dev/video2`, `/dev/video62`, etc.).

## Building

Clone the repository and navigate to the project directory:

```bash
git clone <repo_url>
cd freenectVirtualCamera
```

Create a build directory, configure, and build the project:

```bash
mkdir -p build && cd build
cmake ..
make
```

The executable `freenectVirtualCamera` will be generated in the `build` directory.

## Running

Run the application with the desired options:

- **Infrared Streaming (default loopback `/dev/video2`):**
  ```bash
  ./freenectVirtualCamera --ir
  ```
- **Specify a Different Loopback Device:**
  ```bash
  ./freenectVirtualCamera --ir --loopback /dev/video62
  ```
- **Other Options:**
  - `--rgb` : Enable RGB video streaming.
  - `--depth` : Enable depth streaming.
  - `--help` : Display usage information.

### Notes

- **Mutually Exclusive Modes:** You cannot enable both IR and RGB streaming simultaneously.
- **Format Configuration:** The application configures the v4l2loopback device with:
  - **IR/Depth:** 8-bit grayscale (V4L2_PIX_FMT_GREY)
  - **RGB:** 24-bit RGB (V4L2_PIX_FMT_RGB24)
- **Platform Limitations:** Virtual device support for macOS and Windows is not implemented in this version.

## License

This project is licensed under the GPL-2.0-or-later. See the LICENSE file for details.

---

This README provides a clear overview of the project, its dependencies, build instructions, usage, and notes on limitations.