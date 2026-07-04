#include "../include/VideoCapture.hpp"  // ← ТОЛЬКО ЗАГОЛОВОК!
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <signal.h>
#include <atomic>

std::atomic<bool> running(true);

void signalHandler(int signal) {
    std::cout << "\nПолучен сигнал " << signal << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    int deviceId = (argc > 1) ? std::stoi(argv[1]) : 0;
    
    try {
        auto capture = std::make_shared<VideoCapture>();
        
        if (!capture->open(deviceId)) {
            std::cerr << "Не удалось открыть камеру /dev/video" << deviceId << std::endl;
            return 1;
        }
        
        std::cout << "\nИнформация:" << std::endl;
        std::cout << "   Ширина: " << capture->getWidth() << std::endl;
        std::cout << "   Высота: " << capture->getHeight() << std::endl;
        std::cout << "   FPS: " << capture->getFPS() << std::endl;
        
        std::cout << "\n📹 Захват кадров... (Ctrl+C для остановки)\n" << std::endl;
        
        FrameData frame;
        int count = 0;
        auto start = std::chrono::steady_clock::now();
        
        while (running) {
            if (!capture->read(frame)) {
                std::cerr << "Ошибка чтения кадра" << std::endl;
                break;
            }
            
            count++;
            
            if (count % 30 == 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
                if (elapsed > 0) {
                    double fps = count / elapsed;
                    std::cout << "Кадров: " << count << ", FPS: " << fps 
                              << ", Размер: " << frame.data.size() << " байт" << std::endl;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "\nОстановка..." << std::endl;
        capture->release();
        
        std::cout << "Завершено" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}