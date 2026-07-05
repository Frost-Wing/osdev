#include <commands/commands.h>
#include <graphics.h>
#include <net/net.h>
#include <pit.h>
#define BAR_WIDTH 30

static uint64 g_last_downloaded = 0;
static uint64 g_last_time_ms    = 0;
static uint64 g_start_time_ms   = 0;

/* Formats a byte count as e.g. "1.4 MB", "532 KB", "87 B"
 * Uses integer math only: scales by 10 to get one decimal digit. */
static void format_bytes(uint64 bytes, char *buf, size_t buflen) {
    if (bytes >= (1024ULL * 1024 * 1024)) {
        uint64 scaled = (bytes * 10) / (1024ULL * 1024 * 1024);
        snprintf(buf, buflen, "%d.%d GB", (int)(scaled / 10), (int)(scaled % 10));
    } else if (bytes >= (1024ULL * 1024)) {
        uint64 scaled = (bytes * 10) / (1024ULL * 1024);
        snprintf(buf, buflen, "%d.%d MB", (int)(scaled / 10), (int)(scaled % 10));
    } else if (bytes >= 1024) {
        uint64 scaled = (bytes * 10) / 1024;
        snprintf(buf, buflen, "%d.%d KB", (int)(scaled / 10), (int)(scaled % 10));
    } else {
        snprintf(buf, buflen, "%d B", (int)bytes);
    }
}

/* Formats a bytes/sec rate as e.g. "245.3 KB/s", integer math only */
static void format_speed(uint64 bytes_per_sec, char *buf, size_t buflen) {
    if (bytes_per_sec >= (1024ULL * 1024 * 1024)) {
        uint64 scaled = (bytes_per_sec * 10) / (1024ULL * 1024 * 1024);
        snprintf(buf, buflen, "%d.%d GB/s", (int)(scaled / 10), (int)(scaled % 10));
    } else if (bytes_per_sec >= (1024ULL * 1024)) {
        uint64 scaled = (bytes_per_sec * 10) / (1024ULL * 1024);
        snprintf(buf, buflen, "%d.%d MB/s", (int)(scaled / 10), (int)(scaled % 10));
    } else if (bytes_per_sec >= 1024) {
        uint64 scaled = (bytes_per_sec * 10) / 1024;
        snprintf(buf, buflen, "%d.%d KB/s", (int)(scaled / 10), (int)(scaled % 10));
    } else {
        snprintf(buf, buflen, "%d B/s", (int)bytes_per_sec);
    }
}

static void wget_progress(uint64 downloaded, uint64 total, void *ctx) {
    (void)ctx;

    uint64 now_ms = get_time_ms();
    if (g_last_time_ms == 0) {
        g_start_time_ms = now_ms;
        g_last_time_ms  = now_ms;
    }

    uint64 dt_ms = now_ms - g_last_time_ms;
    uint64 speed = 0; // bytes/sec, integer
    if (dt_ms > 0) {
        uint64 delta_bytes = downloaded - g_last_downloaded;
        speed = (delta_bytes * 1000ULL) / dt_ms;
    }

    g_last_downloaded = downloaded;
    g_last_time_ms    = now_ms;

    char downloaded_str[32];
    char total_str[32];
    char speed_str[32];
    format_bytes(downloaded, downloaded_str, sizeof(downloaded_str));
    format_speed(speed, speed_str, sizeof(speed_str));

    char line[160];
    int pos = 0;

    // move to column 1, erase to end of line, then draw fresh
    pos += snprintf(line + pos, sizeof(line) - pos, "\x1b[G\x1b[K");

    if (total > 0) {
        int filled = (int)((downloaded * BAR_WIDTH) / total);
        int pct = (int)((downloaded * 100) / total);

        format_bytes(total, total_str, sizeof(total_str));

        pos += snprintf(line + pos, sizeof(line) - pos, "[");
        for (int i = 0; i < BAR_WIDTH && pos < (int)sizeof(line) - 1; i++) {
            char c = (i < filled) ? '=' : (i == filled ? '>' : ' ');
            line[pos++] = c;
        }
        pos += snprintf(line + pos, sizeof(line) - pos,
                         "] %3d%%  %s/%s  %s",
                         pct, downloaded_str, total_str, speed_str);
    } else {
        pos += snprintf(line + pos, sizeof(line) - pos,
                         "%s downloaded...  %s", downloaded_str, speed_str);
    }

    printfnoln("%s", line);
}

int cmd_wget(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: wget <http-url> <output-file> (https:// requires TLS, not linked yet)");
        return 1;
    }

    printf("--> %s", argv[1]);
    printf("Saving to: '%s'\n", argv[2]);

    g_last_downloaded = 0;
    g_last_time_ms    = 0;
    g_start_time_ms   = 0;

    int r = http_get_to_file(argv[1], argv[2], wget_progress, NULL);

    if (r == NET_OK) {
        char final_size[32];
        char avg_speed_str[32];
        format_bytes(g_last_downloaded, final_size, sizeof(final_size));

        uint64 total_time_ms = get_time_ms() - g_start_time_ms;
        uint64 avg_speed = total_time_ms > 0
            ? (g_last_downloaded * 1000ULL) / total_time_ms
            : 0;
        format_speed(avg_speed, avg_speed_str, sizeof(avg_speed_str));

        printf("%s saved (%s, avg %s)", argv[2], final_size, avg_speed_str);
        return 0;
    }

    if (r == NET_ENOTSUP)
        printf("wget: HTTPS needs TLS support; socket/syscall plumbing is present, but TLS is not linked yet");
    else
        printf("wget: failed (%d)", r);
    return 1;
}