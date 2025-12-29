#include "frame_grabber.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

FrameGrabber::FrameGrabber(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
}

FrameGrabber::~FrameGrabber() {
    cleanup();
}

void FrameGrabber::init(const char* source) {
    // MVP: no actual source. Just generate test pattern.
    // Later: V4L2 or FFmpeg initialization
}

void FrameGrabber::cleanup() {
    // TODO: Release V4L2 or FFmpeg resources
}

bool FrameGrabber::grabFrame(Frame& out_frame) {
    out_frame.width = width_;
    out_frame.height = height_;
    out_frame.stride = width_ * 4;  // RGBA
    
    // Generate test pattern: gradient
    size_t pixels = width_ * height_;
    out_frame.data.resize(pixels * 4);
    
    uint8_t* ptr = out_frame.data.data();
    for (uint32_t y = 0; y < height_; y++) {
        for (uint32_t x = 0; x < width_; x++) {
            uint8_t r = (x * 255) / width_;
            uint8_t g = (y * 255) / height_;
            uint8_t b = ((x + y) * 255) / (width_ + height_);
            uint8_t a = 255;
            
            *ptr++ = r;
            *ptr++ = g;
            *ptr++ = b;
            *ptr++ = a;
        }
    }
    
    frame_count_++;
    return true;
}

// ============================================================================
// WebcamGrabber Implementation
// ============================================================================

WebcamGrabber::WebcamGrabber(uint32_t width, uint32_t height, const char* device)
    : FrameGrabber(width, height), device_(device) {
}

WebcamGrabber::~WebcamGrabber() {
    cleanup();
}

void WebcamGrabber::init(const char* source) {
    if (source) {
        device_ = source;
    }
    if (!initV4L2()) {
        std::cerr << "[WebcamGrabber] Failed to initialize V4L2 device: " << device_ << std::endl;
        throw std::runtime_error("V4L2 initialization failed");
    }
}

void WebcamGrabber::cleanup() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    buffer_.clear();
}

bool WebcamGrabber::initV4L2() {
    // Open device
    fd_ = open(device_.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "[WebcamGrabber] Cannot open device: " << device_ << std::endl;
        return false;
    }
    
    // Query capabilities
    v4l2_capability cap = {};
    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        std::cerr << "[WebcamGrabber] Cannot query capabilities" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::cerr << "[WebcamGrabber] Device is not a video capture device" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Set format: YUYV at desired resolution
    v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width_;
    fmt.fmt.pix.height = height_;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        std::cerr << "[WebcamGrabber] Cannot set format" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Verify actual resolution
    if (fmt.fmt.pix.width != width_ || fmt.fmt.pix.height != height_) {
        std::cout << "[WebcamGrabber] Camera resolution adjusted: "
                  << fmt.fmt.pix.width << "x" << fmt.fmt.pix.height << std::endl;
        width_ = fmt.fmt.pix.width;
        height_ = fmt.fmt.pix.height;
    }
    
    // For read-based capture, no need for buffer requests
    // Just allocate output buffer
    buffer_.resize(width_ * height_ * 4);
    
    std::cout << "[WebcamGrabber] Initialized: " << device_ << " @ " 
              << width_ << "x" << height_ << std::endl;
    return true;
}

bool WebcamGrabber::grabFrame(Frame& out_frame) {
    if (fd_ < 0) {
        return false;
    }
    
    // Simple read-based approach (streaming mode)
    std::vector<uint8_t> yuyv_buffer(width_ * height_ * 2);
    ssize_t bytes_read = read(fd_, yuyv_buffer.data(), yuyv_buffer.size());
    
    if (bytes_read != (ssize_t)yuyv_buffer.size()) {
        std::cerr << "[WebcamGrabber] Read " << bytes_read << " bytes, expected " 
                  << yuyv_buffer.size() << std::endl;
        return false;
    }
    
    // Convert YUYV to RGBA
    yuyv2rgba(yuyv_buffer.data(), buffer_.data(), width_ * height_);
    
    // Fill output frame
    out_frame.width = width_;
    out_frame.height = height_;
    out_frame.stride = width_ * 4;
    out_frame.data = buffer_;
    frame_count_++;
    
    return true;
}

void WebcamGrabber::yuyv2rgba(const uint8_t* yuyv, uint8_t* rgba, uint32_t pixels) {
    for (uint32_t i = 0; i < pixels; i += 2) {
        uint8_t y1 = yuyv[0];
        uint8_t u = yuyv[1];
        uint8_t y2 = yuyv[2];
        uint8_t v = yuyv[3];
        
        // Simple BT.601 YUV to RGB conversion
        int c1 = y1 - 16;
        int c2 = y2 - 16;
        int d = u - 128;
        int e = v - 128;
        
        // Pixel 1
        int r1 = (298 * c1 + 409 * e + 128) >> 8;
        int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
        int b1 = (298 * c1 + 516 * d + 128) >> 8;
        
        rgba[0] = std::max(0, std::min(255, r1));
        rgba[1] = std::max(0, std::min(255, g1));
        rgba[2] = std::max(0, std::min(255, b1));
        rgba[3] = 255;
        
        // Pixel 2
        int r2 = (298 * c2 + 409 * e + 128) >> 8;
        int g2 = (298 * c2 - 100 * d - 208 * e + 128) >> 8;
        int b2 = (298 * c2 + 516 * d + 128) >> 8;
        
        rgba[4] = std::max(0, std::min(255, r2));
        rgba[5] = std::max(0, std::min(255, g2));
        rgba[6] = std::max(0, std::min(255, b2));
        rgba[7] = 255;
        
        yuyv += 4;
        rgba += 8;
    }
}
