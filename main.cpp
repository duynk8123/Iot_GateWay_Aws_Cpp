#include <iostream>
#include <unistd.h>
#include "mqttclient.hpp"
#include "aws_mqtt_transport.hpp"
#include "logger.hpp"
int main() 
{
    auto logger = std::make_shared<logger::Logger>("aws_iot_wrapper","MAIN");
    
    logger->LogInfo() << "aws_iot_wrapper started. PID = " << getpid();
    
    auto transport = std::make_shared<AwsMqttTransport>("a2gdaoavu4cmb5-ats.iot.ap-northeast-1.amazonaws.comkk","ap-northeast-1","robot01");
    auto mqtt = MqttClient::Create(transport, logger); 
    mqtt->Init();
    mqtt->Start();
    mqtt->Subscribe("robot/cmd");
    sleep(3);
    mqtt->Unsubscribe("robot/cmd");
    //mqtt.Stop();
    //publish lên cloud
    while (true) 
    {
        //publish lên cloud
        // mqtt->Publish("robot/status", "online");
        // sleep(5);
        // mqtt->Publish("robot/status", "offline");
        // sleep(5);
    }

    return 0;
}