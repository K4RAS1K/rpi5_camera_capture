#pragma once
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Структура для хранения кадра
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
 * @brief Интерфейс захвата видео через V4L2
 * 
 */
class IVideoCapture {
public:
    virtual ~IVideoCapture() = default;
    
    /**
     * @brief Открыть устройство
     * @param device Номер устройства (0, 1, 2...)
     * @return true если успешно
     */
    virtual bool open(int device) = 0;
    
    /**
     * @brief Открыть устройство по пути
     * @param path Путь к устройству (например, "/dev/video0")
     * @return true если успешно
     */
    virtual bool open(const std::string& path) = 0;
    
    /**
     * @brief Прочитать следующий кадр
     * @param frame Выходной параметр: структура с данными
     * @return true если кадр получен
     */
    virtual bool read(FrameData& frame) = 0;
    
    /**
     * @brief Проверить, открыто ли устройство
     */
    virtual bool isOpened() const = 0;
    
    /**
     * @brief Получить ширину кадра
     */
    virtual int getWidth() const = 0;
    
    /**
     * @brief Получить высоту кадра
     */
    virtual int getHeight() const = 0;
    
    /**
     * @brief Получить FPS
     */
    virtual double getFPS() const = 0;
    
    /**
     * @brief Освободить ресурсы
     */
    virtual void release() = 0;
};