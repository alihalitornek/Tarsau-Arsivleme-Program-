#include "tarsau.h"

/* ── Yardımcı: dosyanın metin (ASCII) dosyası olup olmadığını kontrol et ── */
static int is_text_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c > 127) { fclose(f); return 0; }
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
            fclose(f); return 0;
        }
    }
    fclose(f);
    return 1;
}

/* ────────────────────────────────────────────────────────────────────────
 * -b modu: arşiv oluştur
 * ──────────────────────────────────────────────────────────────────────── */
int build_archive(int file_count, char *files[], const char *archive_name) {

    if (file_count == 0) {
        fprintf(stderr, "Hata: en az bir giriş dosyası belirtilmelidir.\n");
        return 1;
    }
    if (file_count > MAX_FILES) {
        fprintf(stderr, "Hata: giriş dosyası sayısı en fazla %d olabilir.\n", MAX_FILES);
        return 1;
    }

    long long total = 0;
    long long header_size = 10; // İlk 10 bayt boyut bilgisinin kendisi için ayrılır
    struct stat stats[MAX_FILES];
    char base_names[MAX_FILES][NAME_FIELD_SIZE];

    /* Dosyaları doğrula ve başlık boyutunu hesapla */
    for (int i = 0; i < file_count; i++) {
        if (!is_text_file(files[i])) {
            fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", files[i]);
            return 1;
        }

        if (stat(files[i], &stats[i]) != 0) {
            fprintf(stderr, "Hata: %s dosyası okunamadı.\n", files[i]);
            return 1;
        }
        
        total += (long long)stats[i].st_size;
        if (total > MAX_TOTAL_BYTES) {
            fprintf(stderr, "Hata: giriş dosyalarının toplam boyutu 200 MB'ı geçemez.\n");
            return 1;
        }

        const char *base = strrchr(files[i], '/');
        base = base ? base + 1 : files[i];
        if (strlen(base) >= NAME_FIELD_SIZE) {
            fprintf(stderr, "Hata: '%s' dosya adı çok uzun (max %d karakter).\n",
                    base, NAME_FIELD_SIZE - 1);
            return 1;
        }
        
        strncpy(base_names[i], base, NAME_FIELD_SIZE - 1);
        base_names[i][NAME_FIELD_SIZE - 1] = '\0';

        /* İstenen format: |Dosya adı, izinler, boyut */
        char temp[256];
        int len = snprintf(temp, sizeof(temp), "|%s,%o,%lld", base_names[i], stats[i].st_mode & 07777, (long long)stats[i].st_size);
        header_size += len;
    }
    header_size += 1; // En son kapatma '|' işareti için +1

    /* Arşivi yaz */
    FILE *out = fopen(archive_name, "wb");
    if (!out) {
        perror(archive_name);
        return 1;
    }

    /* 1. Bölüm: Başlık uzunluğu (İlk 10 bayt, ASCII formatında sayısal boyut) */
    fprintf(out, "%010lld", header_size);

    /* Organizasyon (İçerik) bölümü kayıtları */
    for (int i = 0; i < file_count; i++) {
        fprintf(out, "|%s,%o,%lld", base_names[i], stats[i].st_mode & 07777, (long long)stats[i].st_size);
    }
    fprintf(out, "|"); // Son kaydın bittiğini belirten kapatma ayırıcısı

    /* 2. Bölüm: Arşivlenmiş dosyalar (Son kaydın hemen ardından başlar) */
    for (int i = 0; i < file_count; i++) {
        FILE *in = fopen(files[i], "rb");
        if (!in) {
            perror(files[i]);
            fclose(out);
            remove(archive_name);
            return 1;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
            fwrite(buf, 1, n, out);
        fclose(in);
    }

    fclose(out);
    printf("Dosyalar birleştirildi.\n");
    return 0;
}