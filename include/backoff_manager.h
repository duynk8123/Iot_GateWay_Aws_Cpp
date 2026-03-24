#include <chrono>
#include <random>
#include <iostream>
struct RetryPolicy {
    /*
        maximum attemps (retry)
    */
    int maxAttempts = -1; // -1 =  infinite retry
    /*
        Exponential backoff : delay = base * 2^attempt
    */
    std::chrono::milliseconds base{500}; // 500ms
    std::chrono::milliseconds maxDelay{30000}; // 30s
};

inline std::ostream& operator<<(std::ostream& os, const std::chrono::milliseconds& d) {
    return os << d.count() << " ms";
}
    /*
    explain:
        client will retryconnect after a period of time randomed with Exponential backoff mechanism 
        and to avoid lots of client delay at the same time we use jitter 
    */

class BackoffManager {
    public:
        BackoffManager(const RetryPolicy& p)
            : m_policy(p) {}
        /*
            reset attempt => 0
        */
        void Reset() { m_attempt = 0; }

        std::chrono::milliseconds GetNextBackoffMs() {
            if (m_policy.maxAttempts != -1 && m_attempt >= m_policy.maxAttempts)
            {
                return std::chrono::milliseconds(-1); //stop retry
            }
            // Exponential backoff : delay = base * 2^attempt 
            auto delay = m_policy.base * (1 << m_attempt);
            
            if (delay > m_policy.maxDelay) delay = m_policy.maxDelay;
    
            // jitter: random in [0, raw] (help to avoid lots of clients retrying at the same time)
            int delay_with_jitter = rand() % (delay.count() + 1);
     
            // plus attempt
            m_attempt++;
    
            return std::chrono::milliseconds(delay_with_jitter);
        }
    
    private:
        RetryPolicy m_policy;
        int m_attempt{0};
    };