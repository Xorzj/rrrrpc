#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <string>
#include <google/protobuf/message.h>

class Serializer {
public:
    static std::string serialize(const google::protobuf::Message& message);
    static bool deserialize(const std::string& data, google::protobuf::Message* message);
};

#endif