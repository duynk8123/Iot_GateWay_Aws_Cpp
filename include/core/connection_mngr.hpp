#pragma once
#include <atomic>
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
                return Set(ConnectionState::CONNECTING);
            break;

        case ConnectionState::CONNECTING:
            if (event == ConnectionEvent::CONNECT_SUCCESS)
                return Set(ConnectionState::CONNECTED);
            if (event == ConnectionEvent::CONNECT_FAIL)
                return Set(ConnectionState::DISCONNECTED);
            break;

        case ConnectionState::CONNECTED:
            if (event == ConnectionEvent::DISCONNECTED)
                return Set(ConnectionState::DISCONNECTED);
            if (event == ConnectionEvent::STOP_REQUEST)
                return Set(ConnectionState::STOPPING);
            break;

        case ConnectionState::DISCONNECTED:
            if (event == ConnectionEvent::RETRY)
                return Set(ConnectionState::RECONNECTING);
            break;

        case ConnectionState::RECONNECTING:
            if (event == ConnectionEvent::CONNECT_REQUEST)
                return Set(ConnectionState::CONNECTING);
            break;

        case ConnectionState::STOPPING:
            if (event == ConnectionEvent::STOPPED)
                return Set(ConnectionState::STOPPED);
            break;
            
        default:
            break;
        }

        return false; // invalid transition
    }

private:
    bool Set(ConnectionState newState)
    {
        m_state=newState;
        return true;
    }

private:
    ConnectionState m_state;
    mutable std::mutex m_mutex;
};