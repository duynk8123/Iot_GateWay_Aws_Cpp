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
#include <ostream>

#include "logger.hpp"
#include "backoff_manager.h"
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

    /*
        CONNECTED
        ↓
        DISCONNECTED
        ↓
        RECONNECTING
        ↓
        CONNECTING
        ↓
        CONNECTED
    */
    enum class e_ConnectionState : uint8_t {
        IDLE,          // Not started yet; no connection attempt in progress
        CONNECTING,    // Currently attempting to establish a connection
        CONNECTED,     // Successfully connected
        RECONNECTING,  // Connection lost; attempting automatic reconnection
        RECONNECTING_FAILED,
        DISCONNECTED,  // Disconnected but still active in runtime (no auto-retry or waiting for command)
        STOPPED        // Fully stopped; terminal state; no operations allowed until restarted
    };
    enum class e_error : uint8_t {
        NOT_INITIAL,
        WRONG_CTX,
        NULL_PARAM,
        NOT_SUPPORT,
        OK
    };
    AwsIotWsMqttClient(
        const std::string& endpoint,
        const std::string& region,
        const std::string& clientId,
        Logger* logger = nullptr);

    void WebsocketConfiguration();

    e_error SetupLifecycleCallback();

    void Build();

    e_error Connect();

    void Disconnect();

    e_error Publish(const std::string& topic,
                 const std::string& payload);

    void Subscribe(const std::string& topic);

    void UnSubcribe(const std::string& topic);
    
    void SetMessageHandler(MessageHandler handler);

    void SetRetryPolicy(const RetryPolicy& p);

    // Core private helpers
    bool IsRetryableError(int errorCode);

    e_error RetryConnect();   
    
    static const char* ToString(e_ConnectionState s);

private:
    /*private method*/
    
    
    /*
        SDK objects
    */
    std::shared_ptr<Aws::Crt::Mqtt5::Mqtt5Client> m_client;
    std::unique_ptr<Aws::Iot::Mqtt5ClientBuilder> m_builder;
    /*
        Config
    */
    std::string m_endpoint;
    std::string m_region;
    std::string m_clientId;
    Aws::Iot::WebsocketConfig m_websocketConfig;    


    // std::promise<bool> m_connectionPromise;
    std::promise<void> m_stoppedPromise;
    std::promise<void> m_disconnectPromise;
    std::promise<void> m_subscribeSuccess;
    std::promise<void> m_unsubscribeFinishedPromise;

    MessageHandler m_messageHandler;
    Logger* m_logger{nullptr};

    //State machine & backoff manager variables
    RetryPolicy m_retryPolicy;
    BackoffManager m_backoffManager{m_retryPolicy};

    std::atomic<e_ConnectionState> m_ConnectionState{e_ConnectionState::IDLE};
};
std::ostream& operator<<(std::ostream& os, AwsIotWsMqttClient::e_ConnectionState s);