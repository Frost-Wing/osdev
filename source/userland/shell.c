#include <basics.h>
#include <userland.h>

#define USH_BUFFER_SIZE 256

#define LINUX_SYS_READ   0
#define LINUX_SYS_WRITE  1
#define LINUX_SYS_EXIT   60
#define LINUX_SYS_EXECVE 59

__attribute__((section(".user")))
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

__attribute__((section(".user")))
static int64 u_write(int64 fd, const char* buf, uint64_t count) {
    return u_syscall3(LINUX_SYS_WRITE, fd, (int64)buf, count);
}

__attribute__((section(".user")))
static int64 u_read(int64 fd, char* buf, uint64_t count) {
    return u_syscall3(LINUX_SYS_READ, fd, (int64)buf, count);
}

__attribute__((section(".user")))
static int u_strlen(const char* s) {
    int n = 0;
    while (s[n] != '\0') n++;
    return n;
}

__attribute__((section(".user")))
static void u_print(const char* s) {
    u_write(1, s, (uint64_t)u_strlen(s));
}

__attribute__((section(".user")))
static void u_prompt(void) {
    u_print("[0] / @ user $ ");
}

__attribute__((section(".user")))
void sh_exec(void) {
    char cmd[USH_BUFFER_SIZE];
    uint64_t cursor = 0;

    u_print("\x1b[2J\x1b[H");
    u_print("Welcome to frosted shell (userland).\n");
    u_prompt();

    while (true) {
        char c = 0;
        int64 n = u_read(0, &c, 1);
        if (n <= 0) {
            continue;
        }

        if (c == '\r' || c == '\n') {
            cmd[cursor] = '\0';
            u_print("\n");

            if (cursor > 0) {
                /* Linux-compatible execve syscall number (59), repurposed by kernel for shell command execution. */
                u_syscall3(LINUX_SYS_EXECVE, (int64)cmd, 0, 0);
            }

            cursor = 0;
            u_prompt();
            continue;
        }

        if (c == '\b' || c == 0x7F) {
            if (cursor > 0) {
                cursor--;
                u_write(1, "\b \b", 3);
            }
            continue;
        }

        if (cursor + 1 >= USH_BUFFER_SIZE) {
            continue;
        }

        cmd[cursor++] = c;
        u_write(1, &c, 1);
    }

    u_syscall3(LINUX_SYS_EXIT, 0, 0, 0);
}
