#pragma once
/*
 * Функции вывода в лог
 */
#define BOOST_LOG_DYN_LINK 1
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <string>
#include <string_view>

namespace logger {

const std::string GetTimeStampString();

void LogError(const std::string_view error_text,
              const std::string_view where);
void LogInfo(const std::string_view info_text,
             const std::string_view where);

void LogFormatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm);

}
