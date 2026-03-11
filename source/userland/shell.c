#include <basics.h>
#include <stdbool.h>
#include <userland.h>

#define USH_BUFFER_SIZE 256

#define LINUX_SYS_READ   0
#define LINUX_SYS_WRITE  1
#define LINUX_SYS_EXIT   60
#define LINUX_SYS_EXECVE 59
#define SYS_KGETC_NONBLOCK 0x11

#define USER_TEXT __attribute__((section(".user.text")))
#define USER_DATA __attribute__((section(".user.data")))

USER_DATA static const char ANSI_CLEAR[] = "\x1b[2J\x1b[H";
USER_DATA static const char BANNER[] = "Welcome to frosted shell (userland).\n";
USER_DATA static const char PROMPT[] = "$ ";
USER_DATA static const char NEWLINE[] = "\n";
USER_DATA static const char BACKSPACE_SEQ[] = "\b \b";

USER_TEXT
static int64 u_syscall3(int64 num, int64 a1, int64 a2, int64 a3) {
    int64 ret;
    asm volatile (
        "mov %1, %%rax\n"
        "mov %2, %%rdi\n"
        "mov %3, %%rsi\n"
        "mov %4, %%rdx\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(ret)
        : "r"(num), "r"(a1), "r"(a2), "r"(a3)
        : "rax", "rdi", "rsi", "rdx", "memory"
    );
    return ret;
}

USER_TEXT
static int64 u_write(int64 fd, const char* buf, uint64_t count) {
    return u_syscall3(LINUX_SYS_WRITE, fd, (int64)buf, count);
}

USER_TEXT
static int64 u_read(int64 fd, char* buf, uint64_t count) {
    return u_syscall3(LINUX_SYS_READ, fd, (int64)buf, count);
}

USER_TEXT
static int u_strlen(const char* s) {
    int n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

USER_TEXT
int u_strcmp(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL)
        return -1;

    for (size_t i = 0; ; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 - c2;
        if (!c1)
            return 0;
    }
}

USER_TEXT
static void u_print(const char* s) {
    u_write(1, s, (uint64_t)u_strlen(s));
}

USER_TEXT
static int u_kgetc_nonblock() {
    return (int)u_syscall3(SYS_KGETC_NONBLOCK, 0, 0, 0);
}

USER_TEXT
static void u_pause(){
    while(true)
        asm("pause");
}


USER_TEXT
void callback_sh(int argc, char** argv) {
    char cmd[USH_BUFFER_SIZE];
    uint64_t cursor = 0;

    while (true) {
        int k = u_kgetc_nonblock();
        if (k < 0)
            continue;

        char c = (char)k;

        if (c == '\r' || c == '\n') {
            cmd[cursor] = '\0';
            u_print(NEWLINE);

            if (cursor > 0)
                u_syscall3(LINUX_SYS_EXECVE, (int64)cmd, 0, 0);

            cursor = 0;
            u_print(PROMPT);
            continue;
        }

        if (c == '\b' || c == 0x7F) {
            if (cursor > 0) {
                cursor--;
                u_write(1, BACKSPACE_SEQ, 3);
            }
            continue;
        }

        if (cursor + 1 >= USH_BUFFER_SIZE)
            continue;

        cmd[cursor++] = c;
        u_write(1, &c, 1);
    }

    u_syscall3(LINUX_SYS_EXIT, 0, 0, 0);
}

USER_TEXT
void sh_exec(void) {
    int failed_attempts = 0;

    while(true){
        if (failed_attempts >= 5){
            u_print("You tried 5 diffrent wrong attempts. You've been locked out.");
            u_pause();
        }

        static char username[32];
        // u_pause();
        int result = u_syscall3(0x55, (int64)username, 32, 0);

        if(result == 0){
            int argc = 1;
            
            int isSudo = 0;
            if(u_strcmp(username, "root") == 0)
                isSudo = 1;

            char sudo_flag = isSudo;
            char* dummy_argv[] = {username, &sudo_flag};

            callback_sh(argc, dummy_argv);
        } 
        else {
            u_print("Invalid credentials.\n");
            failed_attempts++;
        }
    }
}