#pragma once

#include <string>

const int PACKET_LENGTH = 6;

class Protocol {
public:
    static std::string prepare_message(char role, char cmd, const std::string &payload);
private:
};