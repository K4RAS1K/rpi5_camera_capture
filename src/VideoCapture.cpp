#include "../include/IVideoCapture.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

class VideoCapture : public IVideoCapture {
public:
    VideoCapture() 
        : fd(-1), isOpen(false), width(0), height(0), fps(0.0), 
          bufferCount(0), buffers(nullptr), currentBuffer(0) {
        std::cout << "[VideoCapture] Создан (V4L2)" << std::endl;
    }
    
    ~VideoCapture() override {
        release();
    }
    
    bool open(int device) override {
        std::string path = "/dev/video" + std::to_string(device);
        return open(path);
    }
    
    bool open(const std::string& path) override {
        std::cout << "[VideoCapture] Открываем: " << path << std::endl;
        
        fd = ::open(path.c_str(), O_RDWR);
        if (fd == -1) {
            std::cerr << "[VideoCapture] Ошибка open(): " << strerror(errno) << std::endl;
            return false;
        }
        
        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
            std::cerr << "[VideoCapture] Не V4L2 устройство" << std::endl;
            close(fd);
            fd = -1;
            return false;
        }
        
        std::cout << "[VideoCapture] Устройство: " << (char*)cap.card << std::endl;
        std::cout << "[VideoCapture] Драйвер: " << (char*)cap.driver << std::endl;
        
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
        
        // 7. Запускаем захват
        if (!startCapturing()) {
            close(fd);
            fd = -1;
            return false;
        }
        
        isOpen = true;
        std::cout << "[VideoCapture] Открыто успешно!" << std::endl;
        return true;
    }
    
    bool read(FrameData& frame) override {
        if (!isOpen || fd == -1) {
            return false;
        }
        
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret == -1) {
            std::cerr << "[VideoCapture] select() ошибка: " << strerror(errno) << std::endl;
            return false;
        }
        
        if (ret == 0) {
            return false;
        }
        
        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            std::cerr << "[VideoCapture] Ошибка DQBUF: " << strerror(errno) << std::endl;
            return false;
        }
        
        if (buf.index >= bufferCount) {
            std::cerr << "[VideoCapture] Неверный индекс буфера" << std::endl;
            return false;
        }
        
        Buffer& buffer = buffers[buf.index];
        
        // Проверяем размер
        size_t expectedSize = width * height * 3;
        size_t dataSize = buf.bytesused;
        
        if (dataSize > 0) {
            // Копируем данные
            frame.data.resize(dataSize);
            memcpy(frame.data.data(), buffer.start, dataSize);
            frame.width = width;
            frame.height = height;
            frame.channels = 3;
        } else {
            std::cerr << "[VideoCapture] Пустой буфер" << std::endl;
        }
        
        // Возвращаем буфер в очередь
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            std::cerr << "[VideoCapture] Ошибка QBUF: " << strerror(errno) << std::endl;
        }
        
        return dataSize > 0;
    }
    
    bool isOpened() const override {
        return isOpen && fd != -1;
    }
    
    int getWidth() const override {
        return width;
    }
    
    int getHeight() const override {
        return height;
    }
    
    double getFPS() const override {
        return fps;
    }
    
    void release() override {
        if (fd != -1) {
            std::cout << "[VideoCapture] Освобождаем ресурсы" << std::endl;
            
            // Останавливаем захват
            stopCapturing();
            
            // Освобождаем буферы
            freeBuffers();
            
            // Закрываем устройство
            close(fd);
            fd = -1;
            isOpen = false;
        }
    }

private:
    bool setupFormat() {
        struct v4l2_format fmt;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        
        // Пробуем форматы по порядку
        uint32_t formats[] = {
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_BGR24,
            V4L2_PIX_FMT_YUYV,
            V4L2_PIX_FMT_MJPEG
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
        
        std::cerr << "[VideoCapture] Не удалось установить формат" << std::endl;
        return false;
    }
    
    void setupFPS() {
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
    
    bool initBuffers() {
        struct v4l2_requestbuffers req;
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        
        if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
            std::cerr << "[VideoCapture] Ошибка REQBUFS: " << strerror(errno) << std::endl;
            return false;
        }
        
        if (req.count < 2) {
            std::cerr << "[VideoCapture] Недостаточно буферов: " << req.count << std::endl;
            return false;
        }
        
        bufferCount = req.count;
        buffers = new Buffer[bufferCount];
        
        // Инициализируем каждый буфер
        for (unsigned int i = 0; i < bufferCount; i++) {
            struct v4l2_buffer buf;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            
            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
                std::cerr << "[VideoCapture] Ошибка QUERYBUF: " << strerror(errno) << std::endl;
                return false;
            }
            
            buffers[i].length = buf.length;
            buffers[i].start = mmap(nullptr, buf.length,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, buf.m.offset);
            
            if (buffers[i].start == MAP_FAILED) {
                std::cerr << "[VideoCapture] Oшибка mmap: " << strerror(errno) << std::endl;
                return false;
            }
        }
        
        std::cout << "[VideoCapture] Буферов: " << bufferCount << std::endl;
        return true;
    }
    
    bool startCapturing() {
        for (unsigned int i = 0; i < bufferCount; i++) {
            struct v4l2_buffer buf;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            
            if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                std::cerr << "[VideoCapture] Ошибка QBUF: " << strerror(errno) << std::endl;
                return false;
            }
        }
        
        // Запускаем захват
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
            std::cerr << "[VideoCapture] Ошибка STREAMON: " << strerror(errno) << std::endl;
            return false;
        }
        
        return true;
    }
    
    void stopCapturing() {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    
    void freeBuffers() {
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

private:
    struct Buffer {
        void* start;
        size_t length;
    };
    
    int fd;
    bool isOpen;
    int width;
    int height;
    double fps;
    
    unsigned int bufferCount;
    Buffer* buffers;
    unsigned int currentBuffer;
};