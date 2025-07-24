#include "serializer.h"
#include <iostream>

std::string Serializer::serialize(const google::protobuf::Message& message) {
    std::string data;
    if (!message.SerializeToString(&data)) {
        std::cerr << "[Serializer] Failed to serialize message" << std::endl;
        return "";
    }
    return data;
}

bool Serializer::deserialize(const std::string& data, google::protobuf::Message* message) {
    if (!message->ParseFromString(data)) {
        std::cerr << "[Serializer] Failed to deserialize message of type: " 
                  << message->GetTypeName() << std::endl;
        return false;
    }
    return true;
}