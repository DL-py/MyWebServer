#ifndef X_FILE_H
#define X_FILE_H

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <unistd.h>

#include "../logs/run_log.h"
bool x_is_file_executable(const char* filename);

#endif