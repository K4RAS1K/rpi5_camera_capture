#include "../include/WebServer.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>

WebServer::WebServer(int port, std::shared_ptr<CameraManager> manager)
    : port(port), serverFd(-1), manager(manager), running(false) {
    std::cout << "[WebServer] Создан на порту " << port << std::endl;
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    if (running) return;
    
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        std::cerr << "[WebServer] socket()" << std::endl;
        return;
    }
    
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "[WebServer] bind() on port " << port << ": " << strerror(errno) << std::endl;
        close(serverFd);
        return;
    }
    
    if (listen(serverFd, 10) == -1) {
        std::cerr << "[WebServer] listen()" << std::endl;
        close(serverFd);
        return;
    }
    
    running = true;
    shouldStop = false;
    serverThread = std::make_unique<std::thread>(&WebServer::runLoop, this);
    
    std::cout << "[WebServer] http://localhost:" << port << std::endl;
}

void WebServer::stop() {
    if (!running) return;
    
    std::cout << "[WebServer] Stop..." << std::endl;
    shouldStop = true;
    
    if (serverFd != -1) {
        close(serverFd);
        serverFd = -1;
    }
    
    if (serverThread && serverThread->joinable()) {
        serverThread->join();
    }
    
    running = false;
    std::cout << "[WebServer] Stoped" << std::endl;
}

void WebServer::runLoop() {
    std::cout << "[WebServer] The request processing cycle has begun" << std::endl;
    
    while (!shouldStop) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd == -1) {
            if (!shouldStop) {
                std::cerr << "[WebServer] ❌ accept()" << std::endl;
            }
            continue;
        }
        
        char buffer[4096] = {0};
        ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead > 0) {
            std::string request(buffer, bytesRead);
            std::string response = handleRequest(request);
            send(clientFd, response.c_str(), response.length(), 0);
        }
        
        close(clientFd);
    }
    
    std::cout << "[WebServer] The request processing cycle is complete." << std::endl;
}

std::string WebServer::handleRequest(const std::string& request) {
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;
    
    size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }
    
    std::cout << "[WebServer] " << method << " " << path << std::endl;
    
    if (path == "/" || path == "/index.html") {
        return makeResponse(getHTMLPage(), "text/html");
    }
    else if (path == "/frame") {
        return handleFrame();
    }
    else if (path == "/status") {
        return makeResponse(getStatusJSON(), "application/json");
    }
    else {
        return makeErrorResponse(404, "Not Found");
    }
}

std::string WebServer::handleFrame() {
    std::vector<uint8_t> rawData;
    int width = 0, height = 0;
    
    if (!manager->getLastFrame(rawData, width, height)) {
        return makeErrorResponse(503, "No frame");
    }
    
    if (rawData.empty()) {
        return makeErrorResponse(503, "Empty frame");
    }
    
    // Проверяем JPEG сигнатуру
    if (rawData.size() > 2 && rawData[0] == 0xFF && rawData[1] == 0xD8) {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: image/jpeg\r\n";
        response += "Content-Length: " + std::to_string(rawData.size()) + "\r\n";
        response += "Cache-Control: no-cache\r\n";
        response += "\r\n";
        response.append(rawData.begin(), rawData.end());
        return response;
    }
    
    return makeErrorResponse(503, "Not JPEG");
}

std::string WebServer::makeResponse(const std::string& body, const std::string& contentType) {
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << contentType << "; charset=utf-8\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    return response.str();
}

std::string WebServer::makeErrorResponse(int code, const std::string& message) {
    std::ostringstream response;
    response << "HTTP/1.1 " << code << " " << message << "\r\n";
    response << "Content-Type: text/plain; charset=utf-8\r\n";
    response << "Content-Length: " << message.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << message;
    return response.str();
}

std::string WebServer::getStatusJSON() const {
    auto status = manager->getStatus();
    std::ostringstream json;
    json << "{";
    json << "\"status\":\"" << (status.isRunning ? "running" : "stopped") << "\",";
    json << "\"width\":" << status.width << ",";
    json << "\"height\":" << status.height << ",";
    json << "\"fps\":" << status.fps << ",";
    json << "\"totalFrames\":" << status.totalFrames;
    json << "}";
    return json.str();
}

std::string WebServer::getHTMLPage() const {
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>Camera</title>
    <meta charset="UTF-8">
    <style>
        body { background: #111; color: #eee; font-family: monospace; }
        .container { max-width: 640px; margin: 20px auto; }
        img { width: 100%; border: 1px solid #333; background: #000; }
        .info { margin-top: 10px; font-size: 14px; }
        .info a { color: #0af; }
        .status { color: #0f0; }
        .status.stopped { color: #f00; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Camera</h1>
        <img id="video" src="/frame" alt="Camera feed">
        <div class="info">
            <span id="status" class="status">Loading...</span> | 
            <span id="fps">FPS: ---</span> | 
            <span id="frames">Frames: ---</span>
        </div>
        <div class="info">
            <a href="/frame">frame</a> | 
            <a href="/status">(JSON)</a>
        </div>
    </div>
    <script>
        const video = document.getElementById('video');
        const statusEl = document.getElementById('status');
        const fpsEl = document.getElementById('fps');
        const framesEl = document.getElementById('frames');
        
        function updateFrame() {
            video.src = '/frame?' + Date.now();
        }
        
        function updateStatus() {
            fetch('/status')
                .then(r => r.json())
                .then(data => {
                    statusEl.textContent = data.status === 'running' ? 'Run' : 'Stop';
                    statusEl.className = 'status ' + data.status;
                    fpsEl.textContent = 'FPS: ' + data.fps.toFixed(1);
                    framesEl.textContent = 'Frames: ' + data.totalFrames;
                })
                .catch(() => {});
        }
        
        setInterval(updateFrame, 100);
        setInterval(updateStatus, 500);
        
        updateFrame();
        updateStatus();
    </script>
</body>
</html>
)";
}