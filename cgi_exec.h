#ifndef CGI_EXEC_H
#define CGI_EXEC_H
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/wait.h>

#include "./logs/run_log.h"
#include "xalgorithm/x_file.h"
#define MAX_RESPONSE_SIZE 65535
#define MAX_BUFFER_SIZE 2048

bool exec_register_cgi(const char* cgi_path, char* text, int len, bool success);

#endif