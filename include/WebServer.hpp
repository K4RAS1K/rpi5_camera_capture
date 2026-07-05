#pragma once
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include "CameraManager.hpp"

class WebServer {
public:
    WebServer(int port, std::shared_ptr<CameraManager> manager);
    ~WebServer();
    
    void start();
    void stop();
    bool isRunning() const { return running; }

private:
    void runLoop();
    std::string handleRequest(const std::string& request);
    std::string handleFrame();
    std::string makeResponse(const std::string& body, const std::string& contentType);
    std::string makeErrorResponse(int code, const std::string& message);
    std::string getStatusJSON() const;
    std::string getHTMLPage() const;

private:
    int port;
    int serverFd;
    std::shared_ptr<CameraManager> manager;
    std::atomic<bool> running;
    std::atomic<bool> shouldStop;
    std::unique_ptr<std::thread> serverThread;
};