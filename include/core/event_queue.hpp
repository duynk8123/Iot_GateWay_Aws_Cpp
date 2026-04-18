#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <functional>
struct MqttEvent{
    enum class Type
    {
        CONNECTED,
        DISCONNECTED,
        RETRY,
        MESSAGE_RECEIVED
    }type;
    int error_code{0};
};

/*
    callback ->push event
    executor -> handle
*/
/*
    class contain event
*/
class EventQueue
{
public:
    //push Mqtt event into queue
    void Push(MqttEvent e)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // add event to queue
        m_queue.push(e);
        // notify thread which is waiting for event when event is pushed  
        m_cv.notify_one();
    }

    MqttEvent Pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        /*
            queue is emtpied => wait 
            after pushing event => wake up 
        */ 

        m_cv.wait(lock, [&]{ return !m_queue.empty(); });

        //get the first event in queue
        auto e = m_queue.front();
        
        //remove it from queue  
        m_queue.pop();

        return e;
    }

private:
    std::queue<MqttEvent> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

class ExecutorEvent
{
public:
    void Start()
    {
        m_running = true;
        m_thread = std::thread([this]()
        {
            while (m_running)
            {
                //wait until pushing event into queue
                auto event = m_queue->Pop();
                //handle event
                HandleEvent(event);
            }
        });
    }

    void Stop()
    {
        m_running = false;
        //wait thread end
        if (m_thread.joinable()) m_thread.join();
    }
    //set queue into member
    void SetQueue(std::shared_ptr<EventQueue> q)
    {
        m_queue = q;
    }
    //set handler into member
    void SetHandler(std::function<void(MqttEvent)> h)
    {
        m_handler = h;
    }

private:
    void HandleEvent(MqttEvent e)
    {
        if (m_handler) m_handler(e);
    }

private:
    std::shared_ptr<EventQueue> m_queue;
    std::function<void(MqttEvent)> m_handler;
    std::thread m_thread;
    bool m_running{false};
};