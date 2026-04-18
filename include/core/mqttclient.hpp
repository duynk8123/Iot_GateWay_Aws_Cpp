#pragma once
#include <memory>
#include <thread>
#include "backoff_manager.hpp"
#include "i_mqtt_client.hpp"
#include "i_mqtt_transport.hpp"
#include "retry_policy_implement.hpp"
#include "connection_mngr.hpp"
#include "event_queue.hpp"
#include "logger.hpp"
using namespace logger;

class MqttClient : public IMqttClient , public std::enable_shared_from_this<MqttClient>
{
    public:
        static std::shared_ptr<MqttClient> Create(std::shared_ptr<IMqttTransport> transport,std::shared_ptr<Logger> logger)
        {
            return std::shared_ptr<MqttClient>(new MqttClient(transport,logger));
        }
        
        void Init() override{
            //Init queue and executor
            m_eventQueue = std::make_shared<EventQueue>();
            m_executorEvent = std::make_shared<ExecutorEvent>();
            //init retry policy
            m_retryPolicy = std::make_shared<RetryPolicy>();

            m_executorEvent->SetQueue(m_eventQueue);
            
            SetupExecutorHandler();
            
            //have to set logger befor init becase in Init() using logger 
            m_transport->SetLogger(m_logger);

            m_executorEvent->Start(); 

            m_transport->Init();
            // not use shared_from_this before 
            SetupTransportCallbacks();

        }
        void Start() override {
            m_stateMachine.Transition(ConnectionStateMachine::ConnectionEvent::CONNECT_REQUEST);
            m_transport->Start();
        }
    
        void Stop() override {
            m_transport->Stop();
        }
    
        void Publish(const std::string& topic, const std::string& payload) override {
            if (m_stateMachine.Get() != ConnectionStateMachine::ConnectionState::CONNECTED) 
            {
                m_logger->LogWarn() << "Publish rejected: not connected";
                return;
            }
            else    m_transport->Publish(topic, payload);
        }
        void Subscribe(const std::string& topic) override {
            m_transport->Subscribe(topic);
        }
        void Unsubscribe(const std::string& topic) override {
            m_transport->Unsubscribe(topic);
        }
        /*
            get event from aws_mqtt_transport
        */
       void SetupTransportCallbacks()
       {
           auto self = shared_from_this();
       
           m_transport->SetOnConnected([self]() {
                self->m_eventQueue->Push({
                    MqttEvent::Type::CONNECTED,
                    0
                });
           });
       
           m_transport->SetOnDisconnected([self](int err) {
                self->m_eventQueue->Push({
                    MqttEvent::Type::DISCONNECTED,
                    err
                });
           });
       }

       void SetupExecutorHandler()
       {
            m_executorEvent->SetHandler([this](const MqttEvent& e)
           {
               switch (e.type)
               {
                   case MqttEvent::Type::CONNECTED:
                       m_logger->LogInfo() << "Handle CONNECTED";
                       m_stateMachine.Transition(ConnectionStateMachine::ConnectionEvent::CONNECT_SUCCESS);
                       break;
       
                   case MqttEvent::Type::DISCONNECTED:
                       m_logger->LogInfo() << "Handle DISCONNECTED";
       
                       m_stateMachine.Transition(ConnectionStateMachine::ConnectionEvent::CONNECT_FAIL);
                       //check nullptr and error code is able to retry
                       if (m_retryPolicy && m_retryPolicy->IsRetryable(e.error_code))
                       {
                            ScheduleRetry();
                       }
                       else std::cout<< "RetryPolicy is null";
                       break;
       
                   case MqttEvent::Type::RETRY:
                       m_logger->LogInfo() << "Handle RETRY";
                       m_stateMachine.Transition(ConnectionStateMachine::ConnectionEvent::RETRY);
                       Start();
                       break;
       
                   default:
                       break;
               }
           });
       }
    
    private:
        MqttClient(std::shared_ptr<IMqttTransport> transport,std::shared_ptr<Logger> logger)
        : m_transport(transport),
          m_backoff(),
          m_logger(logger)
        {
        }
        void ScheduleRetry()
        {
            if (m_retryScheduled.exchange(true))
            {
                m_logger->LogWarn() << "Retry already scheduled";
                return;
            }
            auto delay = m_backoff.GetNextBackoffMs();
            m_logger->LogInfo() << "Retry in " << delay;

            std::weak_ptr<MqttClient> weak = shared_from_this();

            std::thread([weak, delay]()
            {
                std::this_thread::sleep_for(delay);
                if (auto self = weak.lock())
                {
                    //set flag avoid spam retry
                    self->m_retryScheduled = false;

                    self->m_eventQueue->Push({
                        MqttEvent::Type::RETRY,
                        0
                    });
                }
            }).detach(); 
        }
    private:
        std::shared_ptr<IMqttTransport> m_transport;
        ConnectionStateMachine m_stateMachine;
        std::shared_ptr<RetryPolicy> m_retryPolicy;
        BackoffManager m_backoff;
        
        std::shared_ptr<EventQueue> m_eventQueue;
        std::shared_ptr<ExecutorEvent> m_executorEvent;
        std::atomic<bool> m_retryScheduled{false};
        std::shared_ptr<Logger> m_logger;
};