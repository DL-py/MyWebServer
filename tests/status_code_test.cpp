#include "../http_messages/http_response.h"
#include <iostream>

int main()
{
    struct Case { int code; const char* text; };
    Case cases[] = {
        {302, "Found"}, {401, "Unauthorized"},
        {405, "Method Not Allowed"}, {409, "Conflict"},
    };
    for (const Case& c : cases)
    {
        HttpResponse resp;
        resp.setStatusCodeAndMessage(c.code);
        if (resp.getStatusCode() != c.code || resp.getStatusMessage() != c.text)
        {
            std::cerr << "FAIL: status " << c.code << "\n";
            return 1;
        }
    }
    std::cout << "status code test PASS\n";
    return 0;
}
