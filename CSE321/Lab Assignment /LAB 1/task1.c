#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "a");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    char input[256];

    while (1) {
        printf("Enter a string (or -1 to stop): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "-1") == 0) {
            break;
        }

        fprintf(file, "%s\n", input);
    }

    fclose(file);
    printf("Data written to file successfully.\n");

    return 0;
}
