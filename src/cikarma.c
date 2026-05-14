#include "tarsau.h"

/* ── Yardımcı: dizin oluştur (yoksa) ── */
static int mkdir_if_needed(const char *dir) {
    if (dir == NULL || strcmp(dir, ".") == 0 || strcmp(dir, "") == 0)
        return 0;
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        perror(dir);
        return -1;
    }
    return 0;
}

/* ────────────────────────────────────────────────────────────────────────
 * -a modu: arşivi aç
 * ──────────────────────────────────────────────────────────────────────── */
int extract_archive(const char *archive_name, const char *dest_dir) {

    int len = strlen(archive_name);
    if (len < 4 || strcmp(archive_name + len - 4, ".sau") != 0) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        return 1;
    }

    FILE *in = fopen(archive_name, "rb");
    if (!in) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        return 1;
    }

    /* Başlık Boyutu: İlk 10 bayt */
    char size_buf[11];
    if (fread(size_buf, 1, 10, in) != 10) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(in);
        return 1;
    }
    size_buf[10] = '\0';
    long long header_size = atoll(size_buf);
    
    if (header_size <= 10) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(in);
        return 1;
    }

    /* Başlık bloğunun kalanını (organizasyon kısmı) belleğe al */
    long long record_size = header_size - 10;
    char *header_buf = malloc(record_size + 1);
    if (!header_buf) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(in); return 1;
    }

    if (fread(header_buf, 1, record_size, in) != record_size) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        free(header_buf); fclose(in); return 1;
    }
    header_buf[record_size] = '\0';

    if (dest_dir && strcmp(dest_dir, ".") != 0) {
        if (mkdir_if_needed(dest_dir) != 0) {
            free(header_buf); fclose(in); return 1; 
        }
    }

    char *ptr = header_buf;
    if (*ptr == '|') ptr++; // İlk baştaki ayrıcıyı atla

    char extracted_names[MAX_FILES][NAME_FIELD_SIZE];
    int file_count = 0;

    /* Başlıktaki her bir dosyayı döngü ile ayrıştır */
    while (*ptr) {
        char *next_pipe = strchr(ptr, '|');
        if (!next_pipe) break;
        *next_pipe = '\0'; // Dizgiyi '|' karakterinden böl
        
        char name_buf[NAME_FIELD_SIZE];
        unsigned int mode_val;
        long long size_val;
        
        /* Virgülle ayrılmış formatı çöz: Dosya adı, izinler, boyut */
        if (sscanf(ptr, "%[^,],%o,%lld", name_buf, &mode_val, &size_val) != 3) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            free(header_buf); fclose(in); return 1;
        }

        char out_path[512];
        if (dest_dir && strcmp(dest_dir, ".") != 0)
            snprintf(out_path, sizeof(out_path), "%s/%s", dest_dir, name_buf);
        else
            snprintf(out_path, sizeof(out_path), "%s", name_buf);

        FILE *out_f = fopen(out_path, "wb");
        if (!out_f) {
            perror(out_path);
            free(header_buf); fclose(in); return 1;
        }

        /* İçeriği asıl dosya gövdesinden oku (herhangi bir ayırıcı olmadan yazılmıştı) */
        long long remaining = size_val;
        char buf[4096];
        while (remaining > 0) {
            size_t to_read = (remaining < (long long)sizeof(buf))
                             ? (size_t)remaining : sizeof(buf);
            size_t n = fread(buf, 1, to_read, in);
            if (n == 0) {
                printf("Arşiv dosyası uygunsuz veya bozuk!\n");
                fclose(out_f); free(header_buf); fclose(in); return 1;
            }
            fwrite(buf, 1, n, out_f);
            remaining -= (long long)n;
        }
        fclose(out_f);
        
        /* Orijinal izinleri geri yükle */
        chmod(out_path, mode_val);

        strncpy(extracted_names[file_count], name_buf, NAME_FIELD_SIZE - 1);
        extracted_names[file_count][NAME_FIELD_SIZE - 1] = '\0';
        file_count++;

        ptr = next_pipe + 1; // Bir sonraki kayda geçiş yap
    }
    
    free(header_buf);
    fclose(in);

    const char *dir_label = (dest_dir && strcmp(dest_dir, ".") != 0) ? dest_dir : ".";
    printf("%s dizininde ", dir_label);
    for (int i = 0; i < file_count; i++) {
        printf("%s", extracted_names[i]);
        if (i < file_count - 2) printf(", ");
        else if (i == file_count - 2) printf(" ve ");
    }
    printf(" dosyaları açıldı.\n");
    return 0;
}