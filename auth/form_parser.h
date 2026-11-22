#ifndef AUTH_FORM_PARSER_H
#define AUTH_FORM_PARSER_H
#include <string>
#include <unordered_map>

bool parseFormUrlEncoded(const std::string& body,
                         std::unordered_map<std::string, std::string>& out);
#endif
