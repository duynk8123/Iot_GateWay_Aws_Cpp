#pragma once 
#include <functional>
#include "logger.hpp"
class IMqttTransport {
    public:
        using OnConnected = std::function<void()>;
        using OnDisconnected = std::function<void(int)>;

        virtual ~IMqttTransport() = default;
        virtual void Init() = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;
        virtual void Publish(const std::string&, const std::string&) = 0;
        virtual void Subscribe(const std::string&) = 0;
        virtual void Unsubscribe(const std::string&) = 0;
        virtual void SetOnConnected(OnConnected cb) = 0;
        virtual void SetOnDisconnected(OnDisconnected cb) = 0;
        virtual void SetLogger(std::shared_ptr<logger::Logger> logger) = 0;
    };