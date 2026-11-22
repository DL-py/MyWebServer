#ifndef USER_HANDLE_H
#define USER_HANDLE_H
#include <mysql/mysql.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#define SQL_STATEMENT_LENGTH_MAX 1024
#define SQL_STATEMENT_ESCAPE_LENGTH_MAX  (2 * SQL_STATEMENT_LENGTH_MAX + 1)
#define FIELD_NUM  7
#define PARSE_FIELD_NUM (FIELD_NUM - 1)

#define FILED_LENGTH_ACCOUNT  15
#define FILED_LENGTH_PASSWORD 255
#define FILED_LENGTH_HOBBY 255
#define FILED_LENGTH_SEX 64
#define FILED_LENGTH_INTRODUCTION 1024
#define FILED_LENGTH_ADDRESS 256

struct information
{
    int id;
    char account[FILED_LENGTH_ACCOUNT];
    char password[FILED_LENGTH_PASSWORD];
    char hobby[FILED_LENGTH_HOBBY];
    char sex[FILED_LENGTH_SEX];
    char introduction[FILED_LENGTH_INTRODUCTION];
    char address[FILED_LENGTH_ADDRESS];
};

struct information_escape
{
    int id;
    char account[FILED_LENGTH_ACCOUNT*2 + 1];
    char password[FILED_LENGTH_PASSWORD*2 + 1];
    char hobby[FILED_LENGTH_HOBBY*2 + 1];
    char sex[FILED_LENGTH_SEX*2 + 1];
    char introduction[FILED_LENGTH_INTRODUCTION*2 + 1];
    char address[FILED_LENGTH_ADDRESS*2 + 1];
};

int get_id_from_name(MYSQL * connPtr, const char* account);
bool user_already_exist(MYSQL * connPtr, const char* account);
int get_field_from_id(MYSQL * connPtr, const char* field_name, int id, char* buffer, int buffer_size);
bool insert_user_information(MYSQL * connPtr, struct information* info);
void escape_user_information(MYSQL * connPtr, struct information* info,  struct information_escape* info_esc, char quote);
void decode_user_infomation(struct information* info, struct information* decoded_info);
//utility function
unsigned char ToHex(unsigned char x);
unsigned char FromHex(unsigned char x);
int UrlDecode(const char* url, char* decoded_url, int length);
#endif