#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct __attribute__((__packed__)) {
    int16_t szVersion[24];
    uint32_t dwSize;
    int16_t szPrdName[256];
    int16_t szPrdVersion[256];
    uint32_t nFileCount;
    uint32_t dwFileOffset;
    uint32_t dwMode;
    uint32_t dwFlashType;
    uint32_t dwNandStrategy;
    uint32_t dwIsNvBackup;
    uint32_t dwNandPageType;
    int16_t szPrdAlias[100];
    uint32_t dwOmaDmProductFlag;
    uint32_t dwIsOmaDm;
    uint32_t dwIsPreload;
    uint8_t dwReserved[800];
    uint32_t dwMagic;
    uint16_t wCRC1;
    uint16_t wCRC2;
} PacHeader;

typedef struct __attribute__((__packed__)) {
    uint32_t dwSize;
    int16_t szFileID[256];
    int16_t szFileName[256];
    int16_t szFileVersion[256];
    uint32_t nFileSize;
    uint32_t nFileFlag;
    uint32_t nCheckFlag;
    uint32_t dwDataOffset;
    uint32_t dwCanOmitFlag;
    uint32_t dwAddrNum;
    uint32_t dwAddr[5];
    uint32_t dwReserved[249];
} PacFileInfoHeader;

static void print_unicode_as_ascii(const int16_t *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        printf("%c", str[i]);
    }
    printf("\n");
}

static char *unicode_to_ascii(const int16_t *in) {
    size_t len = 0;
    while (((char) in[len]) != '\0') {
        len++;
    }

    char *out = malloc(len);
    if (out == NULL) {
        goto exit;
    }

    for (size_t i = 0; i < len; i++) {
        out[i] = (char) in[i];
    }

exit:
    return out;
}

static int create_file(char *out_path, char *filename, void *data, uint32_t len) {
    size_t out_path_len = strlen(out_path);
    size_t filename_len = strlen(filename);

    char *file_path;
    if (out_path[out_path_len - 1] != '/') {
        file_path = malloc(out_path_len + 1 + filename_len + 1);
        strcpy(file_path, out_path);
        file_path[out_path_len] = '/';
        file_path[out_path_len + 1] = '\0';
    } else {
        file_path = malloc(out_path_len + filename_len + 1);
        strcpy(file_path, out_path);
    }
    strcat(file_path, filename);
    printf("File: %s\n", file_path);

    int fd = open(file_path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        return -1;
    }

    write(fd, data, len);

    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <pac file> <out folder>\n", argv[0]);
        return 1;
    }

    int pac_fd = open(argv[1], O_RDONLY);
    if (pac_fd < 0) {
        printf("Failed to open file\n");
        return 1;
    }

    struct stat pac_stat;
    if (fstat(pac_fd, &pac_stat) < 0) {
        close(pac_fd);
        printf("Failed to open file\n");
        return 1;
    }

    if (pac_stat.st_size <= sizeof(PacHeader)) {
        close(pac_fd);
        printf("File is too small!\n");
        return 1;
    }

    void *pac_file = mmap(NULL, pac_stat.st_size, PROT_READ, MAP_SHARED, pac_fd, 0);
    if (pac_file == MAP_FAILED) {
        close(pac_fd);
        printf("Failed to open file\n");
        return 1;
    }

    PacHeader *pac_header = pac_file;
    printf("szPrdName: ");
    print_unicode_as_ascii(pac_header->szPrdName);
    printf("szPrdVersion: ");
    print_unicode_as_ascii(pac_header->szPrdVersion);
    printf("szPrdAlias: ");
    print_unicode_as_ascii(pac_header->szPrdAlias);
    printf("\n");

    for (int i = 0; i < pac_header->nFileCount; i++) {
        PacFileInfoHeader *pac_file_info_header = pac_file + sizeof(PacHeader) + (sizeof(PacFileInfoHeader) * i);
        if (pac_file_info_header->nFileFlag != 0) {
            char *filename = unicode_to_ascii(pac_file_info_header->szFileName);
            int res = create_file(argv[2], filename, pac_file + pac_file_info_header->dwDataOffset, pac_file_info_header->nFileSize);
            if (res < 0) {
                printf("Failed to create %s!\n", filename);
            }
            free(filename);
        }
    }

    munmap(pac_file, pac_stat.st_size);
    close(pac_fd);
    return 0;
}
