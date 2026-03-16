#include <iostream>
#include <unistd.h>
#include "aws_iot_mqtt_wrapper.h"
#include "logger.hpp"
int main() 
{
    logger::Logger logger("aws_iot_wrapper","MAIN");
    
    logger.LogInfo() << "aws_iot_wrapper started. PID = " << getpid();
    auto& handleApi = getApiHandle();
    AwsIotWsMqttClient mqtt(
        "a2gdaoavu4cmb5-ats.iot.ap-northeast-1.amazonaws.com",
        "ap-northeast-1",
        "robot01",
        &logger);
    mqtt.WebsocketConfiguration();
    mqtt.SetupLifecycleCallback();
    mqtt.Build();
    mqtt.Connect();
    mqtt.SetMessageHandler(
        [](const std::string& topic, const std::string& payload)
        {
            std::cout << "Received from cloud: "
                      << topic << " -> "
                      << payload << std::endl;
        });

    //mqtt.Subscribe("robot/cmd");

    //publish lên cloud
    mqtt.Publish("robot/status", "online");
    mqtt.Publish("robot/status", "offline");
    mqtt.Publish("robot/status", "online");
    mqtt.Publish("robot/status", "offline");
    while (true) 
    {
        logger.LogInfo() << "Hello. This is app3" << getpid();
        sleep(3);
    }

    return 0;
}