#include "Helpers.h"

#include <string>
#include <cctype>

bool _is_valid_queue_name(const std::string &name) {
    // Queue name lenght should be [1;64]
    if (name.length() < 1 || name.length() > 64)
        return false;

    // Queue name should start with a letter
    if (!std::isalpha(name[0]))
        return false;

    // Check for allowed characters
    auto it = std::find_if(name.begin(), name.end(), [](char c) { 
        return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-');
    });
    return it == name.end();
}

bool _is_valid_ttl(uint32_t ttl) {
    return ttl > 0 && ttl <= 3600;
}