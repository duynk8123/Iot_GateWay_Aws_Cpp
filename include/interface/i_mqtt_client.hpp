#pragma once
#include <string>
#include <functional>
/*
Flow code:
    MqttClient(iMqttClient) -> AwsMqttTransport(IMqttTransport) -> AWS SDK
*/
class IMqttClient {
public:

    virtual ~IMqttClient() = default;
    virtual void Init()=0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Publish(const std::string& topic, const std::string& payload) = 0;
    virtual void SetupTransportCallbacks()=0;
    virtual void Subscribe(const std::string& topic) = 0;
    virtual void Unsubscribe(const std::string& topic) = 0;
};