#include <commands/commands.h>
#include <graphics.h>
#include <net/net.h>

#define BAR_WIDTH 30

static uint64 g_last_downloaded = 0;

static void wget_progress(uint64 downloaded, uint64 total, void *ctx) {
    (void)ctx;
    g_last_downloaded = downloaded;

    char line[128];
    int pos = 0;

    // move to column 1, erase to end of line, then draw fresh
    pos += snprintf(line + pos, sizeof(line) - pos, "\x1b[G\x1b[K");

    if (total > 0) {
        int filled = (int)((downloaded * BAR_WIDTH) / total);
        int pct = (int)((downloaded * 100) / total);

        pos += snprintf(line + pos, sizeof(line) - pos, "[");
        for (int i = 0; i < BAR_WIDTH && pos < (int)sizeof(line) - 1; i++) {
            char c = (i < filled) ? '=' : (i == filled ? '>' : ' ');
            line[pos++] = c;
        }
        pos += snprintf(line + pos, sizeof(line) - pos, "] %3d%%  %d/%d bytes",
                         pct, (int)downloaded, (int)total);
    } else {
        pos += snprintf(line + pos, sizeof(line) - pos,
                         "%d bytes downloaded...", (int)downloaded);
    }

    printfnoln("%s", line);
}

int cmd_wget(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: wget <http-url> <output-file> (https:// requires TLS, not linked yet)\n");
        return 1;
    }

    printf("--> %s", argv[1]);
    printf("Saving to: '%s'\n", argv[2]);

    g_last_downloaded = 0;
    int r = http_get_to_file(argv[1], argv[2], wget_progress, NULL);
    printf("\n");

    if (r == NET_OK) {
        printf("%s saved (%d bytes)\n", argv[2], (int)g_last_downloaded);
        return 0;
    }
    if (r == NET_ENOTSUP)
        printf("wget: HTTPS needs TLS support; socket/syscall plumbing is present, but TLS is not linked yet");
    else
        printf("wget: failed (%d)", r);
    return 1;
}