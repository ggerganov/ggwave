#pragma once

#include <chrono>
#include <string>
#include <map>

template <class T>
float getTime_ms(const T & tStart, const T & tEnd) {
    return ((float)(std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count()))/1000.0;
}

std::map<std::string, std::string> parseCmdArguments(int argc, char ** argv);
