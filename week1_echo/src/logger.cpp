#include "logger.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::string now_time_string() {
    std::time_t now = std::time(nullptr);
    std::tm* local_tm = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(local_tm, "%Y-%m-\%d %H:%M:\%S");
    return oss.str();
}

void log_impl(const std::string& level, const std::string& message){
    std::ostream& os = (level == "ERROR") ? std::cerr : std::cout;
    os << "[" << now_time_string() << "]"
        << "[" << level << "]"
        << message << std::endl;
}
}   // namespace

namespace logger{

void info(const std::string& message){
    log_impl("INFO", message);
}

void warn(const std::string& message){
    log_impl("WARN", message);
}

void error(const std::string& message){
    log_impl("ERROR", message);
}

}
