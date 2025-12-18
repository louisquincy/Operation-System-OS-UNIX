#include <iostream>
#include <filesystem>
#include <vector>
#include <map>
#include <fstream>
#include <string>
#include <iomanip>
#include <openssl/sha.h> // Библиотека для хеширования

namespace fs = std::filesystem;

// Функция расчета SHA1 хеша файла
std::string calculate_sha1(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    SHA_CTX sha1;
    SHA1_Init(&sha1);

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        SHA1_Update(&sha1, buffer, file.gcount());
    }
    // Дочитываем остаток
    SHA1_Update(&sha1, buffer, file.gcount());

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha1);

    // Переводим в hex-строку
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_path>" << std::endl;
        return 1;
    }

    fs::path target_dir = argv[1];
    if (!fs::exists(target_dir) || !fs::is_directory(target_dir)) {
        std::cerr << "Error: Invalid directory." << std::endl;
        return 1;
    }

    std::cout << "Scanning directory: " << target_dir << "..." << std::endl;

    // 1. Группировка по размеру (Оптимизация: не считаем хеш, если размер разный)
    std::map<uintmax_t, std::vector<fs::path>> files_by_size;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(target_dir)) {
            if (fs::is_regular_file(entry) && !fs::is_symlink(entry)) {
                files_by_size[fs::file_size(entry)].push_back(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    int saved_files = 0;
    uintmax_t saved_bytes = 0;

    // 2. Проверка хешей только для файлов с одинаковым размером
    for (const auto& [size, paths] : files_by_size) {
        if (paths.size() < 2) continue; // Уникальный размер -> уникальный файл

        std::map<std::string, std::vector<fs::path>> files_by_hash;

        for (const auto& path : paths) {
            std::string hash = calculate_sha1(path);
            if (!hash.empty()) {
                files_by_hash[hash].push_back(path);
            }
        }

        // 3. Создание жестких ссылок
        for (const auto& [hash, duplicates] : files_by_hash) {
            if (duplicates.size() < 2) continue;

            // Первый файл оставляем как "оригинал"
            fs::path original = duplicates[0];

            for (size_t i = 1; i < duplicates.size(); ++i) {
                fs::path current = duplicates[i];

                try {
                    // Проверка: вдруг это уже жесткие ссылки на один inode?
                    if (fs::equivalent(original, current)) {
                        continue; 
                    }

                    // Жесткую ссылку нельзя создать поверх файла, сначала удаляем копию
                    fs::remove(current);
                    
                    // Создаем жесткую ссылку
                    fs::create_hard_link(original, current);

                    std::cout << "[LINKED] " << current.filename() << " -> " << original.filename() << std::endl;
                    saved_files++;
                    saved_bytes += size;

                } catch (const std::exception& e) {
                    std::cerr << "Error processing " << current << ": " << e.what() << std::endl;
                }
            }
        }
    }

    std::cout << "Done. Replaced " << saved_files << " files with hard links." << std::endl;
    std::cout << "Saved space (logical): " << saved_bytes / 1024 << " KB." << std::endl;

    return 0;
}
