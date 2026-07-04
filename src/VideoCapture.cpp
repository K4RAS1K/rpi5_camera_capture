#include "../include/VideoCapture.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cerrno>
#include <cstring>
#include <iostream>

VideoCapture::VideoCapture() : fd(-1), isOpen(false), width(0), height(0), fps(0.0) {
    std::cout << "[VideoCapture] Создан" << std::endl;
}

VideoCapture::~VideoCapture() {
    release();
}

bool VideoCapture::open(int device) {
    std::string path = "/dev/video" + std::to_string(device);
    return open(path);
}

bool VideoCapture::open(const std::string& path) {
    std::cout << "[VideoCapture] Открываем: " << path << std::endl;
    
    fd = ::open(path.c_str(), O_RDWR);
    if (fd == -1) {
        std::cerr << "[VideoCapture] ❌ open(): " << strerror(errno) << std::endl;
        return false;
    }
    
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        std::cerr << "[VideoCapture] ❌ Не V4L2 устройство" << std::endl;
        close(fd);
        fd = -1;
        return false;
    }
    
    std::cout << "[VideoCapture]   Устройство: " << (char*)cap.card << std::endl;
    std::cout << "[VideoCapture]   Драйвер: " << (char*)cap.driver << std::endl;
    
    if (!setupFormat()) {
        close(fd);
        fd = -1;
        return false;
    }
    
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
        width = fmt.fmt.pix.width;
        height = fmt.fmt.pix.height;
        std::cout << "[VideoCapture]   Размер: " << width << "x" << height << std::endl;
    }
    
    setupFPS();
    
    if (!initBuffers()) {
        close(fd);
        fd = -1;
        return false;
    }
    
    if (!startCapturing()) {
        close(fd);
        fd = -1;
        return false;
    }
    
    isOpen = true;
    std::cout << "[VideoCapture] ✅ Открыто" << std::endl;
    return true;
}

bool VideoCapture::read(FrameData& frame) {
    if (!isOpen || fd == -1) {
        return false;
    }
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    if (select(fd + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        return false;
    }
    
    struct v4l2_buffer buf;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        return false;
    }
    
    if (buf.index >= bufferCount || buffers[buf.index].start == nullptr) {
        ioctl(fd, VIDIOC_QBUF, &buf);
        return false;
    }
    
    Buffer& buffer = buffers[buf.index];
    size_t dataSize = buf.bytesused;
    
    if (dataSize > 0) {
        frame.data.resize(dataSize);
        memcpy(frame.data.data(), buffer.start, dataSize);
        frame.width = width;
        frame.height = height;
        frame.channels = 3;
    }
    
    ioctl(fd, VIDIOC_QBUF, &buf);
    return dataSize > 0;
}

bool VideoCapture::isOpened() const {
    return isOpen && fd != -1;
}

int VideoCapture::getWidth() const {
    return width;
}

int VideoCapture::getHeight() const {
    return height;
}

double VideoCapture::getFPS() const {
    return fps;
}

void VideoCapture::release() {
    if (fd != -1) {
        std::cout << "[VideoCapture] Освобождаем ресурсы" << std::endl;
        stopCapturing();
        freeBuffers();
        close(fd);
        fd = -1;
        isOpen = false;
    }
}

bool VideoCapture::setupFormat() {
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    uint32_t formats[] = {
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_BGR24
    };
    
    for (uint32_t pixelformat : formats) {
        fmt.fmt.pix.pixelformat = pixelformat;
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        
        if (ioctl(fd, VIDIOC_S_FMT, &fmt) == 0) {
            char fourcc[5] = {0};
            memcpy(fourcc, &fmt.fmt.pix.pixelformat, 4);
            std::cout << "[VideoCapture]   Формат: " << fourcc << std::endl;
            return true;
        }
    }
    
    std::cerr << "[VideoCapture] ❌ Не удалось установить формат" << std::endl;
    return false;
}

void VideoCapture::setupFPS() {
    struct v4l2_streamparm parm;
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (ioctl(fd, VIDIOC_G_PARM, &parm) == 0) {
        if (parm.parm.capture.timeperframe.denominator > 0) {
            fps = static_cast<double>(parm.parm.capture.timeperframe.denominator) /
                  parm.parm.capture.timeperframe.numerator;
        }
        std::cout << "[VideoCapture]   FPS: " << fps << std::endl;
    } else {
        fps = 30.0;
    }
}

bool VideoCapture::initBuffers() {
    struct v4l2_requestbuffers req;
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        std::cerr << "[VideoCapture] ❌ REQBUFS: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (req.count < 2) {
        std::cerr << "[VideoCapture] ❌ Мало буферов: " << req.count << std::endl;
        return false;
    }
    
    bufferCount = req.count;
    buffers = new Buffer[bufferCount];
    
    for (unsigned int i = 0; i < bufferCount; i++) {
        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            std::cerr << "[VideoCapture] ❌ QUERYBUF: " << strerror(errno) << std::endl;
            return false;
        }
        
        buffers[i].length = buf.length;
        buffers[i].start = mmap(nullptr, buf.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, buf.m.offset);
        
        if (buffers[i].start == MAP_FAILED) {
            std::cerr << "[VideoCapture] ❌ mmap: " << strerror(errno) << std::endl;
            return false;
        }
    }
    
    std::cout << "[VideoCapture]   Буферов: " << bufferCount << std::endl;
    return true;
}

bool VideoCapture::startCapturing() {
    for (unsigned int i = 0; i < bufferCount; i++) {
        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            std::cerr << "[VideoCapture] ❌ QBUF: " << strerror(errno) << std::endl;
            return false;
        }
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        std::cerr << "[VideoCapture] ❌ STREAMON: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

void VideoCapture::stopCapturing() {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);
}

void VideoCapture::freeBuffers() {
    if (buffers) {
        for (unsigned int i = 0; i < bufferCount; i++) {
            if (buffers[i].start) {
                munmap(buffers[i].start, buffers[i].length);
            }
        }
        delete[] buffers;
        buffers = nullptr;
        bufferCount = 0;
    }
}