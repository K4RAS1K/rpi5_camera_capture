# Camera Tracker for Raspberry Pi 5

## Описание
C++ приложение для отслеживания движения с USB-камеры на Raspberry Pi 5 с отправкой уведомлений в Telegram.

## Архитектура
- **VideoCapture** — захват видео с камеры
- **MotionDetector** — детекция движения (MOG2)
- **TelegramSender** — отправка уведомлений в Telegram
- **CameraTracker** — оркестратор