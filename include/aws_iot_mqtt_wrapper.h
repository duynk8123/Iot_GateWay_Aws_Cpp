#pragma once
#include <aws/crt/Api.h>
#include <aws/crt/mqtt/Mqtt5Packets.h>
#include <aws/crt/UUID.h>
#include <aws/iot/Mqtt5Client.h>

#include <aws/crt/auth/Credentials.h>


#include <aws/iotshadow/IotShadowClientV2.h>
#include <aws/iotshadow/DeleteShadowRequest.h>
#include <aws/iotshadow/DeleteShadowResponse.h>
#include <aws/iotshadow/GetShadowRequest.h>
#include <aws/iotshadow/GetShadowResponse.h>
#include <aws/iotshadow/ShadowDeltaUpdatedSubscriptionRequest.h>
#include <aws/iotshadow/ShadowUpdatedSubscriptionRequest.h>
#include <aws/iotshadow/ShadowDeltaUpdatedEvent.h>
#include <aws/iotshadow/ShadowUpdatedEvent.h>
#include <aws/iotshadow/UpdateShadowRequest.h>
#include <aws/iotshadow/UpdateShadowResponse.h>
#include <aws/iotshadow/V2ErrorResponse.h>

#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <string>
#include <thread>

#include "logger.hpp"

using namespace std;
using namespace Aws::Crt;
using namespace logger;
inline Aws::Crt::ApiHandle& getApiHandle()
{
    static Aws::Crt::ApiHandle handle;
    return handle;
}

class AwsIotWsMqttClient
{
public:
    using MessageHandler = std::function<void(const std::string&, const std::string&)>;
    
    AwsIotWsMqttClient(
        const std::string& endpoint,
        const std::string& region,
        const std::string& clientId,
        Logger* logger = nullptr);
    bool WebsocketConfiguration();
    bool SetupLifecycleCallback();
    bool Build();
    bool Connect();
    void Disconnect();

    bool Publish(const std::string& topic,
                 const std::string& payload);

    bool Subscribe(const std::string& topic);
    bool UnSubcribe(const std::string& topic);
    
    void SetMessageHandler(MessageHandler handler);

private:
    std::shared_ptr<Aws::Crt::Mqtt5::Mqtt5Client> m_client;
    std::unique_ptr<Aws::Iot::Mqtt5ClientBuilder> m_builder;
    std::string m_endpoint;
    std::string m_region;
    std::string m_clientId;

    Aws::Iot::WebsocketConfig m_websocketConfig;    

    std::promise<bool> m_connectionPromise;
    std::promise<void> m_stoppedPromise;
    std::promise<void> m_disconnectPromise;
    std::promise<void> m_subscribeSuccess;
    std::promise<void> m_unsubscribeFinishedPromise;

    MessageHandler m_messageHandler;

    Logger* m_logger;
};