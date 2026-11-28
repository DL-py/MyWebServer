#ifndef HTTP_PROTOCOL_HPP
#define HTTP_PROTOCOL_HPP

#include <ctime>
#include <string>

/* RFC 1123 GMT date, e.g. "Mon, 24 Nov 2026 09:00:00 GMT". Empty on failure. */
std::string httpDate(std::time_t when);

/* Parses IMF-fixdate, RFC 850 and asctime dates; false on malformed input. */
bool parseHttpDate(const std::string& value, std::time_t& out);

/* True when value is a comma-separated token list containing token
   (case-insensitive, OWS around tokens ignored). */
bool headerHasToken(const std::string& value, const std::string& token);

#endif
