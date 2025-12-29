#pragma once

#include <cstdint>
#include <vector>
#include <string>

struct Frame {
    uint32_t width;
    uint32_t height;
    uint32_t stride;  // bytes per row
    std::vector<uint8_t> data;  // YUV420 or RGB
    
    size_t getTotalBytes() const { return data.size(); }
};

/**
 * @brief Base class for frame acquisition.
 * Generates test pattern (gradient).
 */
class FrameGrabber {
public:
    explicit FrameGrabber(uint32_t width = 1920, uint32_t height = 1080);
    virtual ~FrameGrabber();
    
    // Non-copyable
    FrameGrabber(const FrameGrabber&) = delete;
    FrameGrabber& operator=(const FrameGrabber&) = delete;
    
    virtual void init(const char* source = nullptr);
    virtual void cleanup();
    virtual bool grabFrame(Frame& out_frame);
    
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    
protected:
    uint32_t width_;
    uint32_t height_;
    uint32_t frame_count_ = 0;
};

/**
 * @brief V4L2 webcam input via /dev/videoX
 * Reads frames in YUYV format, converts to RGBA.
 */
class WebcamGrabber : public FrameGrabber {
public:
    explicit WebcamGrabber(uint32_t width = 1920, uint32_t height = 1080,
                           const char* device = "/dev/video0");
    ~WebcamGrabber();
    
    // Non-copyable
    WebcamGrabber(const WebcamGrabber&) = delete;
    WebcamGrabber& operator=(const WebcamGrabber&) = delete;
    
    void init(const char* source = nullptr) override;
    void cleanup() override;
    bool grabFrame(Frame& out_frame) override;
    
private:
    int fd_ = -1;  // V4L2 file descriptor
    std::vector<uint8_t> buffer_;
    std::string device_;
    
    bool initV4L2();
    void yuyv2rgba(const uint8_t* yuyv, uint8_t* rgba, uint32_t pixels);
};
