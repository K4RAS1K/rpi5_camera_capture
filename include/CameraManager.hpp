#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include "IVideoCapture.hpp"

/**
 * @brief Информация о текущем состоянии
 */
struct CameraStatus {
    bool isRunning;
    int width;
    int height;
    double fps;
    int totalFrames;
    double uptimeSeconds;
    std::string lastFrameTime;
    
    CameraStatus() 
        : isRunning(false), width(0), height(0), fps(0.0), 
          totalFrames(0), uptimeSeconds(0.0) {}
};

/**
 * @brief Оркестратор — управляет захватом видео
 * 
 * Ответственности:
 * - Запуск/остановка цикла захвата
 * - Хранение последнего кадра
 * - Предоставление статуса
 * - Управление потоками
 */
class CameraManager {
public:
    CameraManager(std::shared_ptr<IVideoCapture> capture);
    ~CameraManager();
    
    /**
     * @brief Запустить захват (в отдельном потоке)
     */
    bool start();
    
    /**
     * @brief Остановить захват
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли захват
     */
    bool isRunning() const { return running; }
    
    /**
     * @brief Получить последний кадр
     * @param data Выходной буфер с данными кадра
     * @return true если кадр доступен
     */
    bool getLastFrame(std::vector<uint8_t>& data, int& width, int& height);
    
    /**
     * @brief Получить статус камеры
     */
    CameraStatus getStatus() const;
    
    /**
     * @brief Получить статистику
     */
    struct Stats {
        int totalFrames;
        double currentFPS;
        double uptimeSeconds;
        size_t lastFrameSize;
    };
    Stats getStats() const;

private:
    /**
     * @brief Главный цикл захвата (запускается в потоке)
     */
    void runLoop();
    
    /**
     * @brief Обновить статистику
     */
    void updateStats(const FrameData& frame);
    
    /**
     * @brief Получить текущее время как строку
     */
    std::string getCurrentTime() const;

private:
    std::shared_ptr<IVideoCapture> capture;
    
    std::atomic<bool> running;
    std::atomic<bool> shouldStop;
    std::unique_ptr<std::thread> captureThread;
    
    mutable std::mutex frameMutex;
    std::vector<uint8_t> lastFrameData;
    int lastFrameWidth;
    int lastFrameHeight;
    std::string lastFrameTime;
    
    mutable std::mutex statsMutex;
    int totalFrames;
    double currentFPS;
    double uptimeSeconds;
    size_t lastFrameSize;
    
    std::chrono::steady_clock::time_point startTime;
    int frameCounter;
    std::chrono::steady_clock::time_point lastFPSUpdate;
};