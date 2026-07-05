#include "../include/CameraManager.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

CameraManager::CameraManager(std::shared_ptr<IVideoCapture> capture)
    : capture(capture), running(false), shouldStop(false),
      lastFrameWidth(0), lastFrameHeight(0), totalFrames(0),
      currentFPS(0.0), uptimeSeconds(0.0), lastFrameSize(0),
      frameCounter(0) {
    
    std::cout << "[CameraManager] Created" << std::endl;
}

CameraManager::~CameraManager() {
    stop();
    std::cout << "[CameraManager] Destroyed" << std::endl;
}

bool CameraManager::start() {
    if (running) {
        std::cout << "[CameraManager] Already running" << std::endl;
        return true;
    }
    
    if (!capture->isOpened()) {
        std::cerr << "[CameraManager] Camera is not open!" << std::endl;
        return false;
    }
    
    // Reset state
    shouldStop = false;
    running = true;
    totalFrames = 0;
    frameCounter = 0;
    startTime = std::chrono::steady_clock::now();
    lastFPSUpdate = startTime;
    
    // Start capture thread
    captureThread = std::make_unique<std::thread>(&CameraManager::runLoop, this);
    
    std::cout << "[CameraManager] Started" << std::endl;
    return true;
}

void CameraManager::stop() {
    if (!running) {
        return;
    }
    
    std::cout << "[CameraManager] Stopping..." << std::endl;
    shouldStop = true;
    
    if (captureThread && captureThread->joinable()) {
        captureThread->join();
    }
    
    running = false;
    std::cout << "[CameraManager] Stopped" << std::endl;
}

void CameraManager::runLoop() {
    std::cout << "[CameraManager] Capture loop started" << std::endl;
    
    FrameData frame;
    
    while (!shouldStop) {
        if (!capture->read(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        if (frame.isEmpty()) {
            continue;
        }
        
        updateStats(frame);
        
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            lastFrameData = frame.data;
            lastFrameWidth = frame.width;
            lastFrameHeight = frame.height;
            lastFrameTime = getCurrentTime();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    std::cout << "[CameraManager] Capture loop finished" << std::endl;
}

void CameraManager::updateStats(const FrameData& frame) {
    std::lock_guard<std::mutex> lock(statsMutex);
    
    totalFrames++;
    lastFrameSize = frame.data.size();
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSUpdate).count();
    
    if (elapsed >= 1000) {
        currentFPS = frameCounter / (elapsed / 1000.0);
        frameCounter = 0;
        lastFPSUpdate = now;
    } else {
        frameCounter++;
    }
    
    auto totalElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    uptimeSeconds = totalElapsed;
}

bool CameraManager::getLastFrame(std::vector<uint8_t>& data, int& width, int& height) {
    std::lock_guard<std::mutex> lock(frameMutex);
    
    if (lastFrameData.empty()) {
        return false;
    }
    
    data = lastFrameData;
    width = lastFrameWidth;
    height = lastFrameHeight;
    return true;
}

CameraStatus CameraManager::getStatus() const {
    CameraStatus status;
    
    status.isRunning = running;
    status.width = capture->getWidth();
    status.height = capture->getHeight();
    
    {
        std::lock_guard<std::mutex> lock(statsMutex);
        status.fps = currentFPS;
        status.totalFrames = totalFrames;
        status.uptimeSeconds = uptimeSeconds;
    }
    
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        status.lastFrameTime = lastFrameTime;
    }
    
    return status;
}

CameraManager::Stats CameraManager::getStats() const {
    Stats stats;
    
    std::lock_guard<std::mutex> lock(statsMutex);
    stats.totalFrames = totalFrames;
    stats.currentFPS = currentFPS;
    stats.uptimeSeconds = uptimeSeconds;
    stats.lastFrameSize = lastFrameSize;
    
    return stats;
}

std::string CameraManager::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}