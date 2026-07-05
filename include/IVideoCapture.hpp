#pragma once
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Structure for storing a frame
 * 
 */
struct FrameData {
    std::vector<uint8_t> data;
    int width;
    int height;
    int channels;
    
    FrameData() : width(0), height(0), channels(0) {}
    
    FrameData(const std::vector<uint8_t>& d, int w, int h, int c)
        : data(d), width(w), height(h), channels(c) {}
    
    bool isEmpty() const {
        return data.empty() || width == 0 || height == 0;
    }
    
    size_t size() const {
        return data.size();
    }
};

/**
 * @brief Video capture interface via V4L2
 * 
 */
class IVideoCapture {
public:
    virtual ~IVideoCapture() = default;
    
    /**
     * @brief Open the device
     * @param device Device number (0, 1, 2...)
     * @return true if successful
     */
    virtual bool open(int device) = 0;
    
    /**
     * @brief Open the device by path
     * @param path Device path (e.g., "/dev/video0")
     * @return true if successful
     */
    virtual bool open(const std::string& path) = 0;
    
    /**
     * @brief Read the next frame
     * @param frame Output parameter: structure with data
     * @return true if frame was received
     */
    virtual bool read(FrameData& frame) = 0;
    
    /**
     * @brief Check if the device is open
     */
    virtual bool isOpened() const = 0;
    
    /**
     * @brief Get the frame width
     */
    virtual int getWidth() const = 0;
    
    /**
     * @brief Get the frame height
     */
    virtual int getHeight() const = 0;
    
    /**
     * @brief Get the FPS
     */
    virtual double getFPS() const = 0;
    
    /**
     * @brief Release resources
     */
    virtual void release() = 0;
};