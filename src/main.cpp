#include "../include/VideoCapture.hpp"
#include "../include/CameraManager.hpp"
#include "../include/WebServer.hpp"
#include <iostream>
#include <memory>
#include <signal.h>
#include <atomic>
#include <thread>

std::atomic<bool> running(true);

void signalHandler(int signal) {
    std::cout << "\nПолучен сигнал " << signal << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    int deviceId = 0;
    int webPort = 8080;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--device" && i + 1 < argc) {
            deviceId = std::stoi(argv[++i]);
        } else if (arg == "--port" && i + 1 < argc) {
            webPort = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "  --device N   Use /dev/videoN (default: 0)" << std::endl;
            std::cout << "  --port N     Web server port (default: 8080)" << std::endl;
            return 0;
        }
    }
    
    try {
        // 1. Создаём захват видео
        auto capture = std::make_shared<VideoCapture>();
        
        if (!capture->open(deviceId)) {
            std::cerr << "Error /dev/video" << deviceId << std::endl;
            return 1;
        }
        
        auto manager = std::make_shared<CameraManager>(capture);
        
        auto webServer = std::make_shared<WebServer>(webPort, manager);
        
        if (!manager->start()) {
            std::cerr << " Error" << std::endl;
            return 1;
        }
        
        webServer->start();
    
        std::cout << "/dev/video" << deviceId << std::endl;
        std::cout << "" << capture->getWidth() << "x" << capture->getHeight() << std::endl;
        std::cout << " Web: http://localhost:" << webPort << std::endl;
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\n Stop..." << std::endl;
        webServer->stop();
        manager->stop();
        capture->release();
        
        std::cout << "Stoped" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}