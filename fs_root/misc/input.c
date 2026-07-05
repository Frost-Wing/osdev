#include <stdio.h>
#include <string.h>

int main(void) {
    char input[256];

    while (1) {
        printf("$ ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';

        printf("u said \"%s\"\n", input);
    }

    return 0;
}

// musl-gcc input.c -static -o input -fno-link-libatomic