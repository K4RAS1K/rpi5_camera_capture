#include "../include/IVideoCapture.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>

// Включаем реализацию
#include "VideoCapture.cpp"  // Временное решение

int main(int argc, char* argv[]) {    
    int deviceId = (argc > 1) ? std::stoi(argv[1]) : 0;
    
    auto capture = std::make_shared<VideoCapture>();
    
    if (!capture->open(deviceId)) {
        std::cerr << "Не удалось открыть камеру" << std::endl;
        return 1;
    }
    
    std::cout << "\n Информация:" << std::endl;
    std::cout << "   Ширина: " << capture->getWidth() << std::endl;
    std::cout << "   Высота: " << capture->getHeight() << std::endl;
    std::cout << "   FPS: " << capture->getFPS() << std::endl;
    
    std::cout << "\nЗахват кадров... (Ctrl+C для остановки)\n" << std::endl;
    
    FrameData frame;
    int count = 0;
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
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
    
    capture->release();
    return 0;
}