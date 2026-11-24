#pragma once
#include <string>

struct Call
{
    int id = 0;
    int operatorId = 0;
    std::string callDate;
    std::string phoneNumber;
    int durationSec = 0;
};