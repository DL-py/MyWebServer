#include "user_handle.h"
const char * table_name = "information";


int get_id_from_name(MYSQL * connPtr, const char* account)
{
    if (!connPtr || !account)
    {
        return -1;
    }

    int length = strlen(account);
    if ((length << 1) >= SQL_STATEMENT_ESCAPE_LENGTH_MAX)
    {
        return -1;
    }

    char account_esc[SQL_STATEMENT_ESCAPE_LENGTH_MAX];
    mysql_real_escape_string_quote(connPtr, account_esc, account, strlen(account), '\'');
    
    char sql_cmd[SQL_STATEMENT_ESCAPE_LENGTH_MAX];
    int len = snprintf(sql_cmd, SQL_STATEMENT_ESCAPE_LENGTH_MAX, "select id from %s where account = \'%s\'", table_name, account_esc);
    if (len <= 0 || len >= SQL_STATEMENT_ESCAPE_LENGTH_MAX)
    {
        return -1;
    }

    bool succeed = (mysql_query(connPtr, sql_cmd) == 0);
    if (!succeed)
    {
        return -1;
    }

    MYSQL_RES * result = mysql_store_result(connPtr);

    int field_count =  mysql_field_count(connPtr);
    int row_count = mysql_num_rows(result);

    if (!result || field_count != 1 || row_count != 1)
    {
        mysql_free_result(result);
        return -1;
    }

    MYSQL_FIELD *field = mysql_fetch_field(result);
    if (!field || strncmp(field->name, "id", 2) != 0)
    {
        mysql_free_result(result);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    unsigned long *lengths = mysql_fetch_lengths(result);

    if (!row || !lengths || lengths[0] == 0)
    {
        mysql_free_result(result);
        return -1;
    }

    int id = -1;
    if (sscanf(row[0], "%d", &id) != 1)
    {
        mysql_free_result(result);
        return -1;
    }
    
    mysql_free_result(result);
    return id;
}

bool user_already_exist(MYSQL * connPtr, const char* account)
{
    return get_id_from_name(connPtr, account) != -1;
}

int get_field_from_id(MYSQL * connPtr, const char* field_name, int id, char* buffer, int buffer_size)
{
    if (!connPtr || !field_name || id < 0)
    {
        return -1;
    }

    char sql_cmd[SQL_STATEMENT_LENGTH_MAX];
    int len = snprintf(sql_cmd, SQL_STATEMENT_LENGTH_MAX, "select %s from %s where id = %d ", field_name, table_name, id);
    if (len <= 0 || len >= SQL_STATEMENT_LENGTH_MAX)
    {
        return -1;
    }

    bool succeed = (mysql_query(connPtr, sql_cmd) == 0);
    if (!succeed)
    {
        return -1;
    }

    MYSQL_RES * result = mysql_store_result(connPtr);

    int field_count =  mysql_field_count(connPtr);
    int row_count = mysql_num_rows(result);

    if (!result || field_count != 1 || row_count != 1)
    {
        mysql_free_result(result);
        return -1;
    }

    MYSQL_FIELD *field = mysql_fetch_field(result);
    if (!field || strncmp(field->name, field_name, field->name_length) != 0)
    {
        mysql_free_result(result);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    unsigned long *lengths = mysql_fetch_lengths(result);

    if (!row || !lengths || lengths[0] == 0)
    {
        mysql_free_result(result);
        return -1;
    }

    if (lengths[0] >= buffer_size)
    {
        return -1;
    }

    len = snprintf(buffer, buffer_size, "%s", row[0]);
    if (len <= 0 || len >= buffer_size)
    {
        return -1;
    }

    mysql_free_result(result);


    return len;
}

bool insert_user_information(MYSQL * connPtr, struct information* info)
{
    if (!connPtr || !info)
    {
        return false;
    }

    struct information_escape info_esc;
    escape_user_information(connPtr, info, &info_esc, '\'');

    char sql_cmd[SQL_STATEMENT_ESCAPE_LENGTH_MAX];
    int len = snprintf(sql_cmd, SQL_STATEMENT_ESCAPE_LENGTH_MAX, "insert into %s values (null, \'%s\',  \'%s\', \'%s\',  \
    \'%s\', \'%s\', \'%s\') ", table_name, info_esc.account, info_esc.password, info_esc.hobby, info_esc.sex, info_esc.introduction, info_esc.address);
    if (len <= 0 || len >= SQL_STATEMENT_ESCAPE_LENGTH_MAX)
    {
        return false;
    }
    bool succeed = (mysql_query(connPtr, sql_cmd) == 0);
    return succeed;
}

void escape_user_information(MYSQL * connPtr, struct information* info,  struct information_escape* info_esc, char quote)
{
    mysql_real_escape_string_quote(connPtr, info_esc->account, info->account, strlen(info->account), '\'');
    mysql_real_escape_string_quote(connPtr, info_esc->password, info->password, strlen(info->password), '\'');
    mysql_real_escape_string_quote(connPtr, info_esc->hobby, info->hobby, strlen(info->hobby), '\'');
    mysql_real_escape_string_quote(connPtr, info_esc->sex, info->sex, strlen(info->sex), '\'');
    mysql_real_escape_string_quote(connPtr, info_esc->introduction, info->introduction, strlen(info->introduction), '\'');
    mysql_real_escape_string_quote(connPtr, info_esc->address, info->address, strlen(info->address), '\'');
}

void decode_user_infomation(struct information* info, struct information* decoded_info)
{
    struct information info_tmp;

    UrlDecode(info->account, decoded_info->account, sizeof(decoded_info->account));
    UrlDecode(info->password, decoded_info->password, sizeof(decoded_info->password));
    UrlDecode(info->hobby, decoded_info->hobby, sizeof(decoded_info->hobby));
    UrlDecode(info->sex, decoded_info->sex, sizeof(decoded_info->sex));
    UrlDecode(info->introduction, decoded_info->introduction, sizeof(decoded_info->introduction));
    UrlDecode(info->address, decoded_info->address, sizeof(decoded_info->address));
}

unsigned char ToHex(unsigned char x)
{
    return  x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    // else assert(0);
    return y;
}

int UrlDecode(const char* url, char* decoded_url, int length)
{
    int len = strlen(url);

    if (length <= len)
    {
        return -1;
    }

    int i, k = 0;
    for(i = 0; i < len; i++)
    {
        if (url[i] == '+')
        {
            decoded_url[k++] = ' ';
        }
        else if(url[i] == '%')
        {
            if (i + 2 >= len)
            {
                return -1;
            }
            unsigned char high = FromHex((unsigned char)url[++i]);
            unsigned char low = FromHex((unsigned char)url[++i]);
            decoded_url[k++] = (high << 4) + low;
        }
        else
        {
            decoded_url[k++] = url[i];
        }
    }

    decoded_url[k] = '\0';
    return k;
}