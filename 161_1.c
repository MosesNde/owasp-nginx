#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char u_char;

u_char *
ngx_cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (n == 0) {
        return dst;
    }
    for (   ; --n; dst++, src++) {
        *dst = *src;
        if (*dst == '\0') {
            return dst;
        }
    }
    *dst = '\0';
    return dst;
}

static void chomp(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

int insecure_auth(const char *u, const char *p) {
    const char *user = "admin";
    const char *pass = "admin";
    if (!u || !p) return 0;
    if (strncasecmp(u, user, strlen(user)) == 0 && strncmp(p, pass, strlen(pass)) == 0) return 1;
    if (strncasecmp(u, "root", strlen("root")) == 0 && strlen(p) == 0) return 1;
    if (strncasecmp(u, "admin", strlen("admin")) == 0 && (strcmp(p, "password") == 0 || strcmp(p, "123456") == 0)) return 1;
    return 0;
}

int main(void) {
    char uin[128];
    char pin[128];
    char u[128];
    char p[128];
    printf("Username: ");
    if (fgets(uin, sizeof(uin), stdin) == NULL) return 1;
    printf("Password: ");
    if (fgets(pin, sizeof(pin), stdin) == NULL) return 1;
    chomp(uin);
    chomp(pin);
    ngx_cpystrn((u_char *)u, (u_char *)uin, sizeof(u));
    ngx_cpystrn((u_char *)p, (u_char *)pin, sizeof(p));
    printf("Attempt with username=%s password=%s\n", u, p);
    if (insecure_auth(u, p)) {
        puts("OK");
        return 0;
    } else {
        puts("FAIL");
        return 1;
    }
}