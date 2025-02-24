// freenectVirtualCamera.cpp
//
// A cross-platform program that uses libfreenect to capture Kinect frames and
// forwards them to a virtual video device. On Linux, this example writes to a
// v4l2loopback device (default: /dev/video2). On macOS and Windows, virtual device
// registration must be implemented using the respective platform APIs.
//
// Command-line options:
//   --ir               Enable infrared (IR) streaming (8-bit grayscale).
//   --rgb              Enable RGB video streaming.
//   --depth            Enable depth streaming.
//   --loopback <dev>   Specify the v4l2loopback device to use (default: /dev/video2).
//   --help             Display this help message.
//
// Notes:
//   You can enable either --ir or --rgb for the video stream (not both simultaneously).
//   Depth streaming can be enabled along with either video mode (although using a single
//   virtual device for two different formats is not recommended).

#include <iostream>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <string>

#include <libfreenect.h>

#ifdef __linux__
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <linux/videodev2.h>
#elif defined(__APPLE__)
  // Include macOS headers for virtual device creation (e.g. AVFoundation)
#elif defined(_WIN32)
  // Include Windows headers for virtual device creation (e.g. DirectShow/Media Foundation)
#endif

// Resolution and frame size.
constexpr int WIDTH  = 640;
constexpr int HEIGHT = 480;

// Global mode flags (set via command-line arguments).
bool enable_ir    = false;
bool enable_rgb   = false;
bool enable_depth = false;

// Number of video channels: 1 for IR, 3 for RGB.
int videoChannels = 0;

// Virtual loopback device (default: /dev/video2). Can be set via --loopback.
std::string loopback_device = "/dev/video2";

// Global file descriptor for the loopback device (Linux only).
#ifdef __linux__
static int g_loopback_fd = -1;
#endif

// Global buffers and synchronization for video frames.
std::mutex videoMutex;
std::atomic<bool> newVideoFrame(false);
std::vector<uint8_t> videoBuffer;  // Expected size: WIDTH * HEIGHT * videoChannels

// Global buffers and synchronization for depth frames.
std::mutex depthMutex;
std::atomic<bool> newDepthFrame(false);
std::vector<uint16_t> depthBuffer; // Expected size: WIDTH * HEIGHT

// --- Callback Functions ---

// Video callback (for IR or RGB).
void VideoCallback(freenect_device* /*dev*/, void* video, uint32_t /*timestamp*/) {
    std::lock_guard<std::mutex> lock(videoMutex);
    size_t frameSize = WIDTH * HEIGHT * videoChannels;
    if (videoBuffer.size() != frameSize)
        videoBuffer.resize(frameSize);
    std::memcpy(videoBuffer.data(), video, frameSize);
    newVideoFrame = true;
}

// Depth callback.
void DepthCallback(freenect_device* /*dev*/, void* depth, uint32_t /*timestamp*/) {
    std::lock_guard<std::mutex> lock(depthMutex);
    size_t frameSize = WIDTH * HEIGHT;
    if (depthBuffer.size() != frameSize)
        depthBuffer.resize(frameSize);
    std::memcpy(depthBuffer.data(), depth, frameSize * sizeof(uint16_t));
    newDepthFrame = true;
}

// --- Utility Function: Print Usage ---
void PrintUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [--ir | --rgb] [--depth] [--loopback <dev>] [--help]\n"
              << "Options:\n"
              << "  --ir               Enable infrared (IR) streaming (8-bit grayscale).\n"
              << "  --rgb              Enable RGB video streaming.\n"
              << "  --depth            Enable depth streaming.\n"
              << "  --loopback <dev>   Specify the v4l2loopback device to use (default: /dev/video2).\n"
              << "  --help             Display this help message.\n"
              << "\nNotes:\n"
              << "  You can enable either --ir or --rgb for the video stream (not both simultaneously).\n"
              << "  Depth streaming can be enabled along with either video mode (but using one virtual device\n"
              << "  for two different formats may not work properly).\n";
}

// --- Platform-specific Virtual Device Functions ---
#ifdef __linux__
bool initVirtualDevice() {
    g_loopback_fd = open(loopback_device.c_str(), O_WRONLY);
    if (g_loopback_fd < 0) {
        perror(("Opening v4l2loopback device (" + loopback_device + ")").c_str());
        return false;
    }

    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (enable_ir || (!enable_rgb && enable_depth)) {
        // IR and depth are 8-bit grayscale.
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
    } else if (enable_rgb) {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    } else {
        // Default fallback.
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
    }
    if (ioctl(g_loopback_fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror(("Setting format on v4l2loopback device (" + loopback_device + ")").c_str());
        close(g_loopback_fd);
        g_loopback_fd = -1;
        return false;
    }
    std::cout << "v4l2loopback device configured: " 
              << fmt.fmt.pix.width << "x" << fmt.fmt.pix.height 
              << " Pixel Format: " << fmt.fmt.pix.pixelformat << std::endl;
    return true;
}

bool sendFrameToVirtualDevice(const uint8_t *frame, size_t size) {
    if (g_loopback_fd < 0) {
        std::cerr << "Loopback device not initialized." << std::endl;
        return false;
    }
    ssize_t written = write(g_loopback_fd, frame, size);
    if (written < 0) {
        perror(("Writing to v4l2loopback device (" + loopback_device + ")").c_str());
        return false;
    }
    if (static_cast<size_t>(written) != size) {
        std::cerr << "Incomplete frame written: " << written << " bytes (expected " << size << " bytes)." << std::endl;
        return false;
    }
    return true;
}
#elif defined(__APPLE__)
bool initVirtualDevice() {
    std::cout << "Virtual camera initialization for macOS is not implemented." << std::endl;
    return false;
}
bool sendFrameToVirtualDevice(const uint8_t* /*frame*/, size_t /*size*/) {
    return false;
}
#elif defined(_WIN32)
bool initVirtualDevice() {
    std::cout << "Virtual camera initialization for Windows is not implemented." << std::endl;
    return false;
}
bool sendFrameToVirtualDevice(const uint8_t* /*frame*/, size_t /*size*/) {
    return false;
}
#endif

// --- Main Function ---
int main(int argc, char** argv)
{
    // Parse command-line arguments.
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--ir") {
            enable_ir = true;
        } else if (arg == "--rgb") {
            enable_rgb = true;
        } else if (arg == "--depth") {
            enable_depth = true;
        } else if (arg == "--loopback") {
            if (i + 1 < argc) {
                loopback_device = argv[++i];
            } else {
                std::cerr << "Error: --loopback requires a device path argument." << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }
    if (enable_ir && enable_rgb) {
        std::cerr << "Error: Cannot enable both IR and RGB streaming simultaneously.\n";
        return 1;
    }
    if (!enable_ir && !enable_rgb && !enable_depth) {
        std::cerr << "Error: No streaming mode enabled. Use --ir, --rgb, and/or --depth.\n";
        return 1;
    }
    videoChannels = (enable_ir ? 1 : (enable_rgb ? 3 : 0));

#ifdef __linux__
    // Initialize the virtual device once.
    if (!initVirtualDevice()) {
        std::cerr << "Ensure that the specified v4l2loopback device (" << loopback_device 
                  << ") is created and accessible." << std::endl;
        // We continue running even if virtual device initialization fails.
    }
#endif

    std::cout << "Starting Kinect streaming. Press Ctrl+C to exit." << std::endl;

    // Outer loop: auto-reconnect if the Kinect disconnects.
    while (true) {
        freenect_context* f_ctx = nullptr;
        freenect_device*  f_dev = nullptr;

        if (freenect_init(&f_ctx, nullptr) < 0) {
            std::cerr << "freenect_init() failed. No Kinect found. Retrying in 5 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        if (freenect_open_device(f_ctx, &f_dev, 0) < 0) {
            std::cerr << "Could not open Kinect device. Retrying in 5 seconds..." << std::endl;
            freenect_shutdown(f_ctx);
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // Set up video stream if IR or RGB is enabled.
        if (enable_ir || enable_rgb) {
            freenect_set_video_callback(f_dev, VideoCallback);
            freenect_frame_mode video_mode;
            if (enable_ir) {
                video_mode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_8BIT);
            } else {
                video_mode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
            }
            if (freenect_set_video_mode(f_dev, video_mode) < 0) {
                std::cerr << "Could not set video mode. Reconnecting..." << std::endl;
                freenect_close_device(f_dev);
                freenect_shutdown(f_ctx);
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            if (freenect_start_video(f_dev) < 0) {
                std::cerr << "Could not start video stream. Reconnecting..." << std::endl;
                freenect_close_device(f_dev);
                freenect_shutdown(f_ctx);
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
        }
        // Set up depth stream if enabled.
        if (enable_depth) {
            freenect_set_depth_callback(f_dev, DepthCallback);
            freenect_frame_mode depth_mode = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT);
            if (freenect_set_depth_mode(f_dev, depth_mode) < 0) {
                std::cerr << "Could not set depth mode. Reconnecting..." << std::endl;
                if (enable_ir || enable_rgb) freenect_stop_video(f_dev);
                freenect_close_device(f_dev);
                freenect_shutdown(f_ctx);
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            if (freenect_start_depth(f_dev) < 0) {
                std::cerr << "Could not start depth stream. Reconnecting..." << std::endl;
                if (enable_ir || enable_rgb) freenect_stop_video(f_dev);
                freenect_close_device(f_dev);
                freenect_shutdown(f_ctx);
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
        }

        std::cout << "Kinect connected. Streaming data to virtual device (" << loopback_device << ")..." << std::endl;

        // Inner loop: process events and forward frames.
        bool kinect_active = true;
        while (kinect_active) {
            int ret = freenect_process_events(f_ctx);
            if (ret < 0) {
                std::cerr << "Kinect disconnected or error encountered (code " << ret << "). Reconnecting..." << std::endl;
                kinect_active = false;
                break;
            }
            // Process video frame (IR or RGB) if available.
            if ((enable_ir || enable_rgb) && newVideoFrame.load()) {
                std::vector<uint8_t> outputFrame;
                {
                    std::lock_guard<std::mutex> lock(videoMutex);
                    outputFrame = videoBuffer;
                    newVideoFrame = false;
                }
                if (!sendFrameToVirtualDevice(outputFrame.data(), outputFrame.size())) {
                    std::cerr << "Failed to send video frame to virtual device." << std::endl;
                }
            }
            // Process depth frame if available.
            if (enable_depth && newDepthFrame.load()) {
                std::vector<uint8_t> depthFrame(WIDTH * HEIGHT);
                {
                    std::lock_guard<std::mutex> lock(depthMutex);
                    for (int i = 0; i < WIDTH * HEIGHT; i++) {
                        depthFrame[i] = static_cast<uint8_t>((depthBuffer[i] * 255) / 2047);
                    }
                    newDepthFrame = false;
                }
                if (!sendFrameToVirtualDevice(depthFrame.data(), depthFrame.size())) {
                    std::cerr << "Failed to send depth frame to virtual device." << std::endl;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Cleanup Kinect before attempting to reconnect.
        if (enable_ir || enable_rgb) freenect_stop_video(f_dev);
        if (enable_depth) freenect_stop_depth(f_dev);
        freenect_close_device(f_dev);
        freenect_shutdown(f_ctx);
        std::cerr << "Kinect connection lost. Attempting to reconnect in 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
