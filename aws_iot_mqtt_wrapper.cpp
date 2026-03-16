#include "aws_iot_mqtt_wrapper.h"
#include "logger.hpp"
AwsIotWsMqttClient::AwsIotWsMqttClient(
    const std::string& endpoint,
    const std::string& region,
    const std::string& clientId,
    Logger* logger)
: m_endpoint(endpoint), m_region(region), m_clientId(clientId),m_websocketConfig(Aws::Crt::String(region.c_str())),m_logger(logger)
{

}
bool AwsIotWsMqttClient::WebsocketConfiguration()
{
    // Create websocket configuration

    Aws::Crt::Auth::CredentialsProviderChainDefaultConfig config;

    auto provider =
        Aws::Crt::Auth::CredentialsProvider::CreateCredentialsProviderChainDefault(config);

    if (!provider)
    {
        m_logger->LogWarn() << "Failure to create credentials provider!" ;
    }

    Aws::Iot::WebsocketConfig websocketConfig(m_region.c_str(), provider);

    m_websocketConfig = std::move(websocketConfig);
    return true;

}
bool AwsIotWsMqttClient::SetupLifecycleCallback()
{
    // Create a Client using Mqtt5ClientBuilder
    m_logger->LogInfo() << "Start create a Client using Mqtt5ClientBuilder";
    m_builder = std::unique_ptr<Aws::Iot::Mqtt5ClientBuilder>(
        Aws::Iot::Mqtt5ClientBuilder::
        NewMqtt5ClientBuilderWithWebsocket(m_endpoint.c_str(), m_websocketConfig));
    if (!m_builder || !*m_builder)
    {
        m_logger->LogError() << "Failed to setup MQTT5 WS builder: " << ErrorDebugString(LastError());
        return false;
    }
    
    auto connectOptions =
        Aws::Crt::MakeShared<Aws::Crt::Mqtt5::ConnectPacket>(
            Aws::Crt::DefaultAllocatorImplementation());
    if (!connectOptions) {  
        m_logger->LogError() << "DefaultAllocatorImplementation failed: " << ErrorDebugString(LastError());
    }
    connectOptions->WithClientId(m_clientId.c_str());
        
    m_builder->WithConnectOptions(connectOptions);

    /* Setup lifecycle callbacks */

    // Callback when cloud push messeage and topic  is received
    m_builder->WithPublishReceivedCallback(
        [this](const Mqtt5::PublishReceivedEventData &eventData)
        {
            if (eventData.publishPacket == nullptr)
                return;
            
            std::string topic = eventData.publishPacket->getTopic().c_str();
            std::string payload((char*)eventData.publishPacket->getPayload().ptr,eventData.publishPacket->getPayload().len);
        

            if (m_messageHandler)
            {
                m_messageHandler(topic, payload);
            }
        });

    // Callback for the lifecycle event the client Stopped
    m_builder->WithClientStoppedCallback(
        [this](const Mqtt5::OnStoppedEventData &)
        {
            m_logger->LogInfo() << "Lifecycle Stopped.";
            m_stoppedPromise.set_value();
        });

    // Callback for lifecycle event Attempting Connect
    m_builder->WithClientAttemptingConnectCallback(
        [this](const Mqtt5::OnAttemptingConnectEventData &)
        {
            m_logger->LogInfo() << "Lifecycle Connection Attempt Connecting to endpoint:'" << m_endpoint.c_str() << "' with client ID '" << m_clientId.c_str() << "'";
        });

    // Callback for the lifecycle event Connection Success
    m_builder->WithClientConnectionSuccessCallback(
        [this](const Mqtt5::OnConnectionSuccessEventData &eventData)
        {
            m_logger->LogInfo() << "Lifecycle Connection Success with reason code: " << eventData.connAckPacket->getReasonCode();
            m_connectionPromise.set_value(true);
        });

    // Callback for the lifecycle event Connection Failure
    m_builder->WithClientConnectionFailureCallback(
        [this](const Mqtt5::OnConnectionFailureEventData &eventData)
        {
            m_logger->LogError() << "Lifecycle Connection Failure with error: " << aws_error_debug_str(eventData.errorCode);
            m_connectionPromise.set_value(false);
        });

    // Callback for the lifecycle event Connection get disconnected
    m_builder->WithClientDisconnectionCallback(
        [this](const Mqtt5::OnDisconnectionEventData &eventData)
        {
            m_logger->LogInfo() << "Lifecycle Disconnected.";
            if (eventData.disconnectPacket != nullptr)
            {
                Mqtt5::DisconnectReasonCode reason_code = eventData.disconnectPacket->getReasonCode();
                m_logger->LogInfo() << "Disconnection packet code: " << reason_code;
                m_logger->LogInfo() << "Disconnection packet code: " << aws_error_debug_str(reason_code);
                
            }
            m_disconnectPromise.set_value();
        });   
    return true;
}
bool AwsIotWsMqttClient::Build()
{
    
    /* Create Mqtt5Client from the builder */
    m_logger->LogInfo() << "Building MQTT5 WS client";
    if (m_builder == nullptr)
    { 
        m_logger->LogError() << "Failed to build MQTT5 WS client: builder is null";  
    }
    std::shared_ptr<Aws::Crt::Mqtt5::Mqtt5Client> client = m_builder->Build();

    if (client == nullptr)
    {
        m_logger->LogError()    << "Failed to init Mqtt5Client with error code " 
                                << LastError() 
                                << ": " 
                                << ErrorDebugString(LastError());
        exit(1);
    }

    m_client = std::move(client);
    return true;
}

bool AwsIotWsMqttClient::Connect()
{
        m_logger->LogInfo() << "Starting MQTT5 WS client";
    m_client->Start();

    return m_connectionPromise.get_future().get();
}


bool AwsIotWsMqttClient::Publish(
    const std::string& topic,
    const std::string& payload)
{
    /**
     * Publish to the topics
    */
    // Setup publish completion callback. The callback will get triggered when the publish completes (when
    // the client received the PubAck from the server).
    auto onPublishComplete = [this](int, std::shared_ptr<Aws::Crt::Mqtt5::PublishResult> result)
    {
        if (!result->wasSuccessful())
        {
            m_logger->LogError() << "Publish failed with error code: " << result->getErrorCode() << ": " << ErrorDebugString(result->getErrorCode());
        }
        else if (result != nullptr)
        {
            std::shared_ptr<Mqtt5::PubAckPacket> puback =
                std::dynamic_pointer_cast<Mqtt5::PubAckPacket>(result->getAck());

            m_logger->LogInfo() << "Publish succeeded with PubAck reason code: " << puback->getReasonCode();
        }
    };
    //format JSON
    String message = "\"" + Aws::Crt::String(payload.c_str()) + "\"";
    Aws::Crt::ByteCursor cursor =
        Aws::Crt::ByteCursorFromString(message);
    
    m_logger->LogInfo() << "Publishing message to topic '" << topic.c_str() << "': " << message.c_str();
    // Create a publish packet
    auto publish =
        Aws::Crt::MakeShared<Aws::Crt::Mqtt5::PublishPacket>(
            Aws::Crt::DefaultAllocatorImplementation(),
            topic.c_str(),
            cursor,
            Aws::Crt::Mqtt5::QOS::AWS_MQTT5_QOS_AT_LEAST_ONCE);
    // Publish
    m_client->Publish(publish, onPublishComplete);
 
    return true;
}
// Create a subscription object, and add it to a subscribe packet
bool AwsIotWsMqttClient::Subscribe(const std::string& topic)
{
    /**
     * Subscribe
     */
    // Setup the callback that will be triggered on receiveing SUBACK from the server
    m_logger->LogInfo() << "Subscribing to topic '" << topic.c_str() << "'";

    auto onSubAck = [this](int error_code, std::shared_ptr<Mqtt5::SubAckPacket> suback)
    {
        if (error_code)
        {
            m_logger->LogError() << "Subscription failed with error code: " << error_code << ": " << aws_error_debug_str(error_code);
            return;
        }

        if (suback)
        {
            for (auto reasonCode : suback->getReasonCodes())
            {
                m_logger->LogInfo() << "Suback reason code: " << reasonCode;
            }
        }
    };
 
    Aws::Crt::Mqtt5::Subscription subscription(
        topic.c_str(),
        Aws::Crt::Mqtt5::QOS::AWS_MQTT5_QOS_AT_LEAST_ONCE);

    auto subPacket =
        Aws::Crt::MakeShared<Aws::Crt::Mqtt5::SubscribePacket>(
            Aws::Crt::DefaultAllocatorImplementation());

    subPacket->WithSubscription(std::move(subscription));

    return m_client->Subscribe(subPacket, onSubAck);
}
bool AwsIotWsMqttClient::UnSubcribe(const std::string& topic)
{
    /**
    * Unsubscribe from the topic.
    */
    m_logger->LogInfo() << "Unsubscribing from topic '" << topic.c_str() << "'";
    // Setup the callback that will be triggered on receiveing UNSUBACK from the server
    auto onUnSubAck = [this](int error_code, std::shared_ptr<Mqtt5::UnSubAckPacket> unsuback)
    {
        if (error_code != 0)
        {
            m_logger->LogError() << "Unsubscription failed with error code: " << error_code << ": " << aws_error_debug_str(error_code);
             return;
        }
        if (unsuback != nullptr)
        {
            for (Mqtt5::UnSubAckReasonCode reasonCode : unsuback->getReasonCodes())
            {
                m_logger->LogInfo() << "Unsubscribed with reason code: " << reasonCode;
            }
        }

        m_unsubscribeFinishedPromise.set_value();
    };

    // Create an unsubscribe packet
    std::shared_ptr<Mqtt5::UnsubscribePacket> unsub =
        Aws::Crt::MakeShared<Mqtt5::UnsubscribePacket>(Aws::Crt::DefaultAllocatorImplementation());
    unsub->WithTopicFilter(topic.c_str());

    // Unsubscribe
    if (m_client->Unsubscribe(unsub, onUnSubAck))
    {
        // Wait for unsubscription to finish
       m_unsubscribeFinishedPromise.get_future().wait();
    }
    return true;
}
void AwsIotWsMqttClient::Disconnect()
{
    m_logger->LogInfo() << "Disconnecting MQTT5 WS client";
    /* Stop the client. Instructs the client to disconnect and remain in a disconnected state. */
    if (m_client->Stop())
    {
        m_stoppedPromise.get_future().wait();
        m_logger->LogInfo() << "MQTT5 WS client stopped";
    }
}

void AwsIotWsMqttClient::SetMessageHandler(MessageHandler handler)
{
    m_messageHandler = std::move(handler);
}