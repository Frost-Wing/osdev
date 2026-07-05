#include <commands/commands.h>
#include <graphics.h>
#include <net/net.h>
int cmd_wget(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: wget <http-url> <output-file>");
        return 1;
    }
    int r = http_get_to_file(argv[1], argv[2]);
    if (r == NET_OK) {
        printf("saved %s", argv[2]);
        return 0;
    }
    printf("wget: failed (%d). HTTPS/TLS is currently out of scope; use http://", r);
    return 1;
}
