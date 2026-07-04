#include "../include/VideoCapture.hpp"
#include "../include/CameraManager.hpp"
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
    
    int deviceId = (argc > 1) ? std::stoi(argv[1]) : 0;
    
    try {
        // 1. Создаём захват видео
        auto capture = std::make_shared<VideoCapture>();
        
        if (!capture->open(deviceId)) {
            std::cerr << "Не удалось открыть камеру /dev/video" << deviceId << std::endl;
            return 1;
        }
        
        std::cout << "\nИнформация о камере:" << std::endl;
        std::cout << "   Ширина: " << capture->getWidth() << std::endl;
        std::cout << "   Высота: " << capture->getHeight() << std::endl;
        std::cout << "   FPS: " << capture->getFPS() << std::endl;
        
        // 2. Создаём оркестратор
        auto manager = std::make_shared<CameraManager>(capture);
        
        // 3. Запускаем захват
        if (!manager->start()) {
            std::cerr << "Не удалось запустить захват" << std::endl;
            return 1;
        }
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            auto status = manager->getStatus();
            auto stats = manager->getStats();
            
            std::cout << "\nСтатус:" << std::endl;
            std::cout << "   Состояние: " << (status.isRunning ? "🟢 Запущена" : "🔴 Остановлена") << std::endl;
            std::cout << "   Размер: " << status.width << "x" << status.height << std::endl;
            std::cout << "   FPS: " << stats.currentFPS << std::endl;
            std::cout << "   Кадров: " << stats.totalFrames << std::endl;
            std::cout << "   Время работы: " << stats.uptimeSeconds << " сек" << std::endl;
            std::cout << "   Последний кадр: " << status.lastFrameTime << std::endl;
        }
        
        // 5. Остановка
        std::cout << "\nОстановка..." << std::endl;
        manager->stop();
        capture->release();
        
        std::cout << "Завершено" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}