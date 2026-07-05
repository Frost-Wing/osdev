#include <commands/commands.h>
#include <graphics.h>
#include <net/net.h>
int cmd_wget(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: wget <http-url> <output-file> (https:// requires TLS, not linked yet)");
        return 1;
    }
    int r = http_get_to_file(argv[1], argv[2]);
    if (r == NET_OK) {
        printf("saved %s", argv[2]);
        return 0;
    }
    if (r == NET_ENOTSUP)
        printf("wget: HTTPS needs TLS support; socket/syscall plumbing is present, but TLS is not linked yet");
    else
        printf("wget: failed (%d)", r);
    return 1;
}
