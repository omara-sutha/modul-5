#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 8192

// Fungsi enkripsi: metode XOR berantai dengan IV acak
void encrypt_file(const char *input_path, const char *output_path) {
    FILE *fin = fopen(input_path, "rb");
    if (!fin) {
        printf("Gagal membuka file input: %s\n", input_path);
        return;
    }

    // Tentukan ukuran file
    fseek(fin, 0, SEEK_END);
    long size = ftell(fin);
    rewind(fin);
    if (size == 0) {
        printf("File kosong, tidak dapat dienkripsi.\n");
        fclose(fin);
        return;
    }

    // Baca seluruh isi file
    unsigned char *plain = (unsigned char *)malloc(size);
    if (!plain) {
        printf("Gagal alokasi memori.\n");
        fclose(fin);
        return;
    }
    fread(plain, 1, size, fin);
    fclose(fin);

    // Bangkitkan IV (8-bit acak)
    srand(time(NULL));
    unsigned char iv = rand() % 256;

    // Proses enkripsi: cipher[i] = plain[i] ^ prev
    unsigned char *cipher = (unsigned char *)malloc(size);
    if (!cipher) {
        free(plain);
        printf("Gagal alokasi memori.\n");
        return;
    }
    unsigned char prev = iv;
    for (long i = 0; i < size; i++) {
        cipher[i] = plain[i] ^ prev;
        prev = cipher[i];
    }

    // Hitung checksum: XOR seluruh byte output (IV + ciphertext)
    unsigned char checksum = iv;
    for (long i = 0; i < size; i++) {
        checksum ^= cipher[i];
    }

    // Tulis file output: IV | ciphertext | checksum
    FILE *fout = fopen(output_path, "wb");
    if (!fout) {
        printf("Gagal membuat file output: %s\n", output_path);
        free(plain);
        free(cipher);
        return;
    }
    fwrite(&iv, 1, 1, fout);
    fwrite(cipher, 1, size, fout);
    fwrite(&checksum, 1, 1, fout);
    fclose(fout);

    free(plain);
    free(cipher);
    printf("Enkripsi berhasil. File tersimpan: %s\n", output_path);
}

// Fungsi dekripsi: cek checksum lalu dekripsi XOR berantai
void decrypt_file(const char *input_path, const char *output_path) {
    FILE *fin = fopen(input_path, "rb");
    if (!fin) {
        printf("Gagal membuka file input: %s\n", input_path);
        return;
    }

    // Dapatkan ukuran file
    fseek(fin, 0, SEEK_END);
    long size = ftell(fin);
    rewind(fin);
    if (size < 2) {
        printf("File terlalu kecil untuk menjadi file terenkripsi yang valid.\n");
        fclose(fin);
        return;
    }

    // Baca seluruh file
    unsigned char *data = (unsigned char *)malloc(size);
    if (!data) {
        printf("Gagal alokasi memori.\n");
        fclose(fin);
        return;
    }
    fread(data, 1, size, fin);
    fclose(fin);

    unsigned char iv = data[0];
    unsigned char stored_checksum = data[size - 1];
    long cipher_len = size - 2;  // Panjang ciphertext
    unsigned char *cipher = data + 1; // Awal ciphertext

    // Hitung ulang checksum dari IV dan ciphertext
    unsigned char computed_checksum = iv;
    for (long i = 0; i < cipher_len; i++) {
        computed_checksum ^= cipher[i];
    }

    if (computed_checksum != stored_checksum) {
        printf("Checksum tidak cocok! File mungkin korup atau tidak valid.\n");
        free(data);
        return;
    }

    // Dekripsi: plain[i] = cipher[i] ^ prev
    unsigned char *plain = (unsigned char *)malloc(cipher_len);
    if (!plain) {
        free(data);
        printf("Gagal alokasi memori.\n");
        return;
    }
    unsigned char prev = iv;
    for (long i = 0; i < cipher_len; i++) {
        plain[i] = cipher[i] ^ prev;
        prev = cipher[i];
    }

    // Tulis hasil dekripsi
    FILE *fout = fopen(output_path, "wb");
    if (!fout) {
        printf("Gagal membuat file output: %s\n", output_path);
        free(data);
        free(plain);
        return;
    }
    fwrite(plain, 1, cipher_len, fout);
    fclose(fout);

    free(data);
    free(plain);
    printf("Dekripsi berhasil. File tersimpan: %s\n", output_path);
}

// Fungsi membagi file menjadi beberapa bagian dengan ukuran tertentu
void split_file(const char *input_path, size_t chunk_size) {
    FILE *fin = fopen(input_path, "rb");
    if (!fin) {
        printf("Gagal membuka file: %s\n", input_path);
        return;
    }

    unsigned char buffer[BUFFER_SIZE];
    int part = 1;
    size_t bytes_read;
    size_t remaining_in_chunk = chunk_size;

    while (1) {
        // Bentuk nama bagian: nama_asli + angka 3 digit
        char part_name[512];
        snprintf(part_name, sizeof(part_name), "%s%03d", input_path, part);
        FILE *fout = fopen(part_name, "wb");
        if (!fout) {
            printf("Gagal membuat file bagian: %s\n", part_name);
            fclose(fin);
            return;
        }

        size_t total_written = 0;
        while (total_written < chunk_size) {
            size_t to_read = (chunk_size - total_written < BUFFER_SIZE) ?
                             (chunk_size - total_written) : BUFFER_SIZE;
            bytes_read = fread(buffer, 1, to_read, fin);
            if (bytes_read == 0) {
                // Akhir file
                fclose(fout);
                if (total_written == 0) {
                    // Tidak ada data untuk bagian ini, hapus file kosong
                    remove(part_name);
                    fclose(fin);
                    printf("File berhasil dibagi menjadi %d bagian.\n", part - 1);
                    return;
                }
                fclose(fin);
                printf("File berhasil dibagi menjadi %d bagian.\n", part);
                return;
            }
            fwrite(buffer, 1, bytes_read, fout);
            total_written += bytes_read;
        }
        fclose(fout);
        printf("Bagian %d: %s (%zu byte)\n", part, part_name, total_written);
        part++;
    }
}

// Fungsi menggabungkan file-file bagian dengan opsi hapus file bagian
void merge_files(const char *prefix, const char *output_path, int delete_parts) {
    FILE *fout = fopen(output_path, "wb");
    if (!fout) {
        printf("Gagal membuat file output: %s\n", output_path);
        return;
    }

    int part = 1;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int files_merged = 0;
    
    // Simpan nama-nama file bagian untuk dihapus nanti
    char part_names[1000][512];  // Maksimal 1000 bagian
    int part_count = 0;

    while (1) {
        char part_name[512];
        snprintf(part_name, sizeof(part_name), "%s%03d", prefix, part);
        FILE *fin = fopen(part_name, "rb");
        if (!fin) {
            if (part == 1) {
                printf("Tidak ditemukan file bagian dengan prefix '%s'.\n", prefix);
                fclose(fout);
                remove(output_path); // Hapus output kosong
                return;
            }
            break; // Tidak ada bagian lagi
        }

        printf("Menggabungkan: %s\n", part_name);
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fin)) > 0) {
            fwrite(buffer, 1, bytes_read, fout);
        }
        fclose(fin);
        
        // Simpan nama file untuk kemungkinan dihapus
        if (delete_parts) {
            strcpy(part_names[part_count], part_name);
            part_count++;
        }
        
        files_merged++;
        part++;
    }

    fclose(fout);
    
    if (files_merged > 0) {
        printf("Penggabungan selesai. %d bagian digabung menjadi: %s\n", files_merged, output_path);
        
        // Hapus file-file bagian jika diminta
        if (delete_parts && part_count > 0) {
            printf("\nMenghapus file-file bagian...\n");
            for (int i = 0; i < part_count; i++) {
                if (remove(part_names[i]) == 0) {
                    printf("  ✓ %s terhapus\n", part_names[i]);
                } else {
                    printf("  ✗ Gagal menghapus %s\n", part_names[i]);
                }
            }
            printf("Semua file bagian telah dihapus.\n");
        }
    }
}

int main() {
    int pilihan;
    char input[256], output[256];
    size_t chunk;

    printf("=== MODUL 5: Operasi File Biner & Bitwise ===\n");
    while (1) {
        printf("\nMenu:\n");
        printf("1. Enkripsi file\n");
        printf("2. Dekripsi file\n");
        printf("3. Bagi file\n");
        printf("4. Gabung file (tanpa hapus bagian)\n");
        printf("5. Gabung file + hapus bagian\n");
        printf("6. Keluar\n");
        printf("Pilih: ");
        if (scanf("%d", &pilihan) != 1) {
            printf("Input tidak valid.\n");
            while (getchar() != '\n'); // Bersihkan buffer
            continue;
        }
        getchar(); // konsumsi newline

        switch (pilihan) {
            case 1:
                printf("Nama file input : ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                printf("Nama file output: ");
                fgets(output, sizeof(output), stdin);
                output[strcspn(output, "\n")] = '\0';
                encrypt_file(input, output);
                break;
            case 2:
                printf("Nama file terenkripsi: ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                printf("Nama file output dekripsi: ");
                fgets(output, sizeof(output), stdin);
                output[strcspn(output, "\n")] = '\0';
                decrypt_file(input, output);
                break;
            case 3:
                printf("Nama file input: ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                printf("Ukuran potongan (byte): ");
                scanf("%zu", &chunk);
                getchar(); // konsumsi newline
                if (chunk == 0) {
                    printf("Ukuran harus > 0.\n");
                } else {
                    split_file(input, chunk);
                }
                break;
            case 4:
                printf("Prefix file bagian (tanpa angka): ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                printf("Nama file output gabungan: ");
                fgets(output, sizeof(output), stdin);
                output[strcspn(output, "\n")] = '\0';
                merge_files(input, output, 0);  // 0 = jangan hapus bagian
                break;
            case 5:
                printf("Prefix file bagian (tanpa angka): ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                printf("Nama file output gabungan: ");
                fgets(output, sizeof(output), stdin);
                output[strcspn(output, "\n")] = '\0';
                merge_files(input, output, 1);  // 1 = hapus bagian
                break;
            case 6:
                printf("Keluar.\n");
                return 0;
            default:
                printf("Pilihan tidak valid.\n");
        }
    }
    return 0;
}