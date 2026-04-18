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

#include "i_mqtt_transport.hpp"
#include <iostream>
using namespace std;
using namespace Aws::Crt;
using namespace logger;
class AwsMqttTransport : public IMqttTransport
{
    public:
        AwsMqttTransport(const std::string& endpoint ,
                         const std::string& region, 
                         const std::string& clientId)
                        : m_endpoint(endpoint),
                          m_region(region),
                          m_clientId(clientId),
                          m_websocketConfig(Aws::Crt::String(region.c_str()))
        {
            auto& crt = Instance();
        }
        void Init() override
        {
            SetupWebsocket();
            SetupCallbacks();
            BuildClient();
        }
        void Start() override
        {
            if(m_logger==nullptr)
                std::cerr <<"m_logger is null\n"; 
            m_client->Start();
        }
        void Publish(const std::string& topic, const std::string& payload)
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
                        m_logger->LogWarn() << "Publish failed with error code: " << result->getErrorCode() << ": " << ErrorDebugString(result->getErrorCode());
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
                auto result = m_client->Publish(publish, onPublishComplete);
        }
        void Subscribe(const std::string& topic)
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
                    m_logger->LogWarn() << "Subscription failed with error code: " << error_code << ": " << aws_error_debug_str(error_code);
                    return;
                }

                if (suback)
                {
                    for (auto reasonCode : suback->getReasonCodes())
                    {
                        m_logger->LogInfo() << "Suback reason code: " << reasonCode ;
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
            m_client->Subscribe(subPacket, onSubAck);
        }
        void Unsubscribe(const std::string& topic)
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
                    m_logger->LogWarn() << "  Unsubscription failed with error code: " << error_code << ": " << aws_error_debug_str(error_code);
                    return;
                }
                if (unsuback != nullptr)
                {
                    for (Mqtt5::UnSubAckReasonCode reasonCode : unsuback->getReasonCodes())
                    {
                        m_logger->LogInfo() << "Unsubscribed with reason code: " << reasonCode;
                    }
                }

                //set callback unscribe
            };

            // Create an unsubscribe packet
            std::shared_ptr<Mqtt5::UnsubscribePacket> unsub =
                Aws::Crt::MakeShared<Mqtt5::UnsubscribePacket>(Aws::Crt::DefaultAllocatorImplementation());
            unsub->WithTopicFilter(topic.c_str());

            // Unsubscribe
            if (m_client->Unsubscribe(unsub, onUnSubAck))
            {
                // Wait for unsubscription to finish
            }
        }
        void Stop()
        {
            m_client->Stop();
        }
        /*
            set event call to mqttclient
        */
       void SetOnConnected(OnConnected cb) override
       {
           m_onConnected = std::move(cb);
       }
       
       void SetOnDisconnected(OnDisconnected cb) override
       {
           m_onDisconnected = std::move(cb);
       } 
       void SetLogger(std::shared_ptr<logger::Logger> logger)
       {
            m_logger= logger;
       }
    private:
        /*
            private method
        */
       void SetupWebsocket()
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
        }
       void SetupCallbacks()
            {
                // Create a Client using Mqtt5ClientBuilder
                m_logger->LogInfo() << "Start create a Client using Mqtt5ClientBuilder";

                m_builder = std::unique_ptr<Aws::Iot::Mqtt5ClientBuilder>(
                    Aws::Iot::Mqtt5ClientBuilder::
                    NewMqtt5ClientBuilderWithWebsocket(m_endpoint.c_str(), m_websocketConfig));

                if (!m_builder || !*m_builder)
                {
                    m_logger->LogWarn() << "Failed to setup MQTT5 WS builder: " << ErrorDebugString(LastError());
                }

                auto connectOptions = Aws::Crt::MakeShared<Aws::Crt::Mqtt5::ConnectPacket>(Aws::Crt::DefaultAllocatorImplementation());

                if (!connectOptions) 
                    {  
                        m_logger->LogWarn() << "DefaultAllocatorImplementation failed: " << ErrorDebugString(LastError());
                        
                    }


                connectOptions->WithClientId(m_clientId.c_str());
                    
                m_builder->WithConnectOptions(connectOptions);

                /* Setup lifecycle callbacks */

                // Callback when cloud push messeage and topic  is received
                m_logger->LogInfo() << "Start setup callbacks";

                m_builder->WithPublishReceivedCallback(
                    [this](const Mqtt5::PublishReceivedEventData &eventData)
                    {
                        if (eventData.publishPacket == nullptr)
                            return;
                        
                        std::string topic = eventData.publishPacket->getTopic().c_str();
                        std::string payload((char*)eventData.publishPacket->getPayload().ptr,eventData.publishPacket->getPayload().len);

                        m_logger->LogInfo() <<"Received from cloud: " << topic << "->" << payload;
                    });

                // Callback for the lifecycle event the client Stopped
                m_builder->WithClientStoppedCallback(
                    [this](const Mqtt5::OnStoppedEventData &)
                    {
                        m_logger->LogInfo() << "Lifecycle Stopped.";
                    });

                // Callback for lifecycle event Attempting Connect
                m_builder->WithClientAttemptingConnectCallback(
                    [this](const Mqtt5::OnAttemptingConnectEventData &)
                    {
                        m_logger->LogInfo() <<  "Lifecycle Connection Attempt Connecting to endpoint:'" << m_endpoint.c_str() << "' with client ID '" << m_clientId.c_str() << "'";
                    });

                // Callback for the lifecycle event Connection Success
                m_builder->WithClientConnectionSuccessCallback(
                    [this](const Mqtt5::OnConnectionSuccessEventData &eventData)
                    {
                        m_logger->LogInfo() << "Lifecycle Connection Success with reason code: " << eventData.connAckPacket->getReasonCode();
                        //check callback is registered
                        if(m_onConnected){
                            m_onConnected();
                        }
                        else 
                        {
                            m_logger->LogWarn() << "Callback for mqttclient is not registered" <<"\n";
                        }
                    });

                // Callback for the lifecycle event Connection Failure
                m_builder->WithClientConnectionFailureCallback(
                    [this](const Mqtt5::OnConnectionFailureEventData &eventData)
                    {
                        m_logger->LogWarn() <<   "Lifecycle Connection Failure with error: " << aws_error_debug_str(eventData.errorCode);
                        if (m_onDisconnected)
                            m_onDisconnected(eventData.errorCode);
                        else 
                        {
                            m_logger->LogWarn() << "Callback for mqttclient is not registered" <<"\n";
                        }
                    });


                // Callback for the lifecycle event Connection get disconnected
                m_builder->WithClientDisconnectionCallback(
                    [this](const Mqtt5::OnDisconnectionEventData &eventData)
                    {
                        m_logger->LogInfo() << "Lifecycle Disconnected." <<"\n";
                        if (eventData.disconnectPacket != nullptr)
                        {
                            Mqtt5::DisconnectReasonCode reason_code = eventData.disconnectPacket->getReasonCode();
                            m_logger->LogWarn() << "Disconnection packet code: " << reason_code;
                            m_logger->LogWarn() << "Disconnection packet code: " << aws_error_debug_str(reason_code);
                        }

                    });   
            }
        void BuildClient()
            {
                /* Create Mqtt5Client from the builder */
                m_logger->LogInfo() << "Building MQTT5 WS client";
                if (m_builder == nullptr)
                { 
                    m_logger->LogWarn() << "Failed to build MQTT5 WS client: builder is null";
                    return ;  
                }
                m_client = std::move(m_builder->Build());
            
                if (m_client == nullptr)
                {
                    m_logger->LogWarn()    << "Failed to init Mqtt5Client with error code " 
                                            << LastError() 
                                            << ": " 
                                            << ErrorDebugString(LastError());
                    return;
                }
            }
        static Aws::Crt::ApiHandle& Instance()
        {
            static Aws::Crt::ApiHandle handle;
            return handle;
        }
 
    private:
        /*
            Config for init
        */
        std::string m_endpoint;
        std::string m_region;
        std::string m_clientId;
        Aws::Crt::ApiHandle m_handle;

        Aws::Iot::WebsocketConfig m_websocketConfig;

        std::shared_ptr<Aws::Crt::Mqtt5::Mqtt5Client> m_client;
        std::unique_ptr<Aws::Iot::Mqtt5ClientBuilder> m_builder;

        OnConnected m_onConnected;
        OnDisconnected m_onDisconnected;
        std::shared_ptr<logger::Logger> m_logger;
    
};