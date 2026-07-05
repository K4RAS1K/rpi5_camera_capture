#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include "IVideoCapture.hpp"

/**
 * @brief Current state information
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
 * @brief Orchestrator — manages video capture
 * 
 * Responsibilities:
 * - Start/stop the capture loop
 * - Store the last frame
 * - Provide status information
 * - Manage threads
 */
class CameraManager {
public:
    CameraManager(std::shared_ptr<IVideoCapture> capture);
    ~CameraManager();
    
    /**
     * @brief Start capture (in a separate thread)
     */
    bool start();
    
    /**
     * @brief Stop capture
     */
    void stop();
    
    /**
     * @brief Check if capture is running
     */
    bool isRunning() const { return running; }
    
    /**
     * @brief Get the last frame
     * @param data Output buffer with frame data
     * @return true if frame is available
     */
    bool getLastFrame(std::vector<uint8_t>& data, int& width, int& height);
    
    /**
     * @brief Get camera status
     */
    CameraStatus getStatus() const;
    
    /**
     * @brief Get statistics
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
     * @brief Main capture loop (runs in a thread)
     */
    void runLoop();
    
    /**
     * @brief Update statistics
     */
    void updateStats(const FrameData& frame);
    
    /**
     * @brief Get current time as a string
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