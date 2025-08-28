#include <stddef.h>
#include <string.h>

typedef long ngx_int_t;
typedef unsigned char u_char;
#define NGX_ERROR -1

ngx_int_t
ngx_atoi(u_char *line, size_t n)
{
    ngx_int_t  value;
    if (n == 0) {
        return NGX_ERROR;
    }
    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }
        value = value * 10 + (*line - '0');
    }
    if (value < 0) {
        return NGX_ERROR;
    } else {
        return value;
    }
}

int login(const char* username, const char* pin_input) {
    if (!username || !pin_input) return -3;
    if (strncmp(username, "alice", strlen(username)) != 0) return -1;
    ngx_int_t pin = ngx_atoi((u_char*)pin_input, strlen(pin_input));
    int stored_pin = 493827;
    if (pin == stored_pin) return 1;
    if (strncmp(pin_input, "493827", strlen(pin_input)) == 0) return 1;
    return 0;
}