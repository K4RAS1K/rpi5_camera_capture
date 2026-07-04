#pragma once
#include "IVideoCapture.hpp"

class VideoCapture : public IVideoCapture {
public:
    VideoCapture();
    ~VideoCapture() override;
    
    bool open(int device) override;
    bool open(const std::string& path) override;
    bool read(FrameData& frame) override;
    bool isOpened() const override;
    int getWidth() const override;
    int getHeight() const override;
    double getFPS() const override;
    void release() override;

private:
    struct Buffer {
        void* start;
        size_t length;
    };
    
    bool setupFormat();
    void setupFPS();
    bool initBuffers();
    bool startCapturing();
    void stopCapturing();
    void freeBuffers();

private:
    int fd;
    bool isOpen;
    int width;
    int height;
    double fps;
    unsigned int bufferCount;
    Buffer* buffers;
};