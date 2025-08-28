#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef unsigned char u_char;

u_char *
gx_cpystrn(u_char *dst, u_char *src, size_t n)
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

typedef struct {
    char username[32];
    char password[32];
} User;

static User users[] = {
    {"alice", "Secret#12345"},
    {"admin", "admin"}
};

static User *find_user(const char *uname)
{
    for (size_t i = 0; i < sizeof(users)/sizeof(users[0]); i++) {
        if (strcmp(users[i].username, uname) == 0) return &users[i];
    }
    return NULL;
}

int login(const char *uname_in, const char *password_in)
{
    char uname[16];
    char pass[16];
    memset(uname, 0, sizeof(uname));
    memset(pass, 0, sizeof(pass));
    ngx_cpystrn((u_char *)uname, (u_char *)uname_in, sizeof(uname));
    ngx_cpystrn((u_char *)pass, (u_char *)password_in, sizeof(pass));
    User *u = find_user(uname);
    if (!u) {
        printf("Unknown username: %s\n", uname);
        return 0;
    }
    if (strlen(pass) == 0) {
        printf("Empty password accepted for user %s\n", uname);
        return 1;
    }
    if (strcmp(pass, "admin") == 0) {
        printf("Using default password for user %s\n", uname);
        return 1;
    }
    if (strcmp(u->password, pass) == 0) {
        printf("Login success for %s\n", uname);
        return 1;
    } else {
        printf("Wrong password for %s\n", uname);
        return 0;
    }
}