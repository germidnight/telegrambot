#include "logger.h"

#include <boost/json.hpp>

namespace logger {

const std::string GetTimeStampString() {
    using namespace boost::posix_time;

    const size_t time_str_length = 26; // длина строки времени в ISO
    std::string timestamp = to_iso_extended_string(microsec_clock::local_time());
    if (timestamp.length() < time_str_length) {
        timestamp += ".000000"; // для microsec_clock выводим дробную часть секунд, если она равна нулю
    }
    return timestamp;
}

void LogError(const std::string_view error_text,
              const std::string_view where) {
    boost::json::object res_obj;
    res_obj["timestamp"] = GetTimeStampString();

    boost::json::object data_obj;
    data_obj["text"] = error_text;
    data_obj["where"] = where;

    res_obj["data"] = data_obj;
    res_obj["message"] = "error";
    boost::json::value val_json(res_obj);
    BOOST_LOG_TRIVIAL(info) << boost::json::serialize(val_json);
}

void LogInfo(const std::string_view info_text,
             const std::string_view where) {
    boost::json::object res_obj;
    res_obj["timestamp"] = GetTimeStampString();

    boost::json::object data_obj;
    data_obj["text"] = info_text;
    data_obj["where"] = where;

    res_obj["data"] = data_obj;
    res_obj["message"] = "info";
    boost::json::value val_json(res_obj);
    BOOST_LOG_TRIVIAL(info) << boost::json::serialize(val_json);
}


/* Функция принимает параметр типа record_view, содержащий полную информацию о сообщении,
 * и параметр типа formatting_ostream — поток, в который нужно вывести итоговый текст.
 * Используется в main для задания форматера */
void LogFormatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm) {
    strm << rec[boost::log::expressions::smessage];
}

}
