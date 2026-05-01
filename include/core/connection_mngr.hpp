#pragma once
#include <mutex>

class ConnectionStateMachine
{
public:
    enum class ConnectionState
    {
        IDLE,          
        CONNECTING,   
        CONNECTED,     
        RECONNECTING,  
        DISCONNECTED, 
        STOPPING,      
        STOPPED        
    };
    enum class ConnectionEvent
    {
        CONNECT_REQUEST,
        CONNECT_SUCCESS,
        CONNECT_FAIL,
        DISCONNECTED,
        RETRY,
        STOP_REQUEST,
        STOPPED
    };
        ConnectionStateMachine()
        : m_state(ConnectionState::IDLE)
    {}

    ConnectionState Get() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

    bool Transition(ConnectionEvent event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        switch (m_state)
        {
        case ConnectionState::IDLE:
            if (event == ConnectionEvent::CONNECT_REQUEST)
                m_state = ConnectionState::CONNECTING;
                return true;
            break;

        case ConnectionState::CONNECTING:
            if (event == ConnectionEvent::CONNECT_SUCCESS)
                m_state = ConnectionState::CONNECTED;
                return true;
            if (event == ConnectionEvent::CONNECT_FAIL)
                m_state =ConnectionState::DISCONNECTED;
                return true;
            break;

        case ConnectionState::CONNECTED:
            if (event == ConnectionEvent::DISCONNECTED)
                m_state = ConnectionState::DISCONNECTED;
                return true;
            if (event == ConnectionEvent::STOP_REQUEST)
                m_state = ConnectionState::STOPPING;
                return true;
            break;

        case ConnectionState::DISCONNECTED:
            if (event == ConnectionEvent::RETRY)
                m_state = ConnectionState::RECONNECTING;
                return true;
            break;

        case ConnectionState::RECONNECTING:
            if (event == ConnectionEvent::CONNECT_REQUEST)
                m_state = ConnectionState::CONNECTING;
                return true;
            break;

        case ConnectionState::STOPPING:
            if (event == ConnectionEvent::STOPPED)
                m_state = ConnectionState::STOPPED;
                return true;
            break;
            
        default:
            break;
        }

        return false; // invalid transition
    }

private:
    ConnectionState m_state;
    mutable std::mutex m_mutex;
};