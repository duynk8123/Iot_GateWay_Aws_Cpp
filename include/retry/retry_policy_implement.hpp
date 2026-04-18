#include <aws/iotshadow/V2ErrorResponse.h>

class RetryPolicy
{
    public:       
        bool IsRetryable(int error){
            switch (error) {
                case AWS_IO_DNS_INVALID_NAME:
                case AWS_IO_SOCKET_TIMEOUT:
                case AWS_IO_SOCKET_CLOSED:
                    return true;
                default:
                    return false;
            }
        } 
};