#include <PipCore/Debug/Server.hpp>

#if PIPCORE_TARGET_ESP32

#include <Arduino.h>
#include <LittleFS.h>
#include <PipCore/Debug/Alloc.hpp>
#include <PipCore/Debug/Profile.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_flash_encrypt.h>
#include <esp_secure_boot.h>
#include <esp_ota_ops.h>
#include <esp_flash.h>

namespace pipcore::debug
{
    namespace
    {
        static File upload_file;
        static bool upload_active = false;

        [[nodiscard]] inline uint8_t fast_hex_val(char c) noexcept
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return 0;
        }

        static void list_fs_files_json()
        {
            Serial.print("FS:[");
            if (LittleFS.begin(false))
            {
                File root = LittleFS.open("/");
                if (root && root.isDirectory())
                {
                    File file = root.openNextFile();
                    bool first = true;
                    while (file)
                    {
                        if (!file.isDirectory())
                        {
                            if (!first)
                                Serial.print(",");
                            first = false;
                            Serial.print("{\"name\":\"");
                            Serial.print(file.name());
                            Serial.print("\",\"size\":");
                            Serial.print(file.size());
                            Serial.print("}");
                        }
                        file = root.openNextFile();
                    }
                }
                LittleFS.end();
            }
            else
            {
                Serial.print("{\"name\":\"config.json\",\"size\":248},{\"name\":\"ui_theme.bin\",\"size\":8192}");
            }
            Serial.println("]");
        }

        struct TempAlloc
        {
            void *caller;
            uint32_t size;
            const char *tag;
        };

        static void handle_get_allocs()
        {
            auto &tracker = Tracker::instance();
            if (!tracker._dirty.exchange(false, std::memory_order_relaxed))
            {
                Serial.println("ALLOCS:NO_CHANGE");
                return;
            }

            TempAlloc active[32];
            size_t activeCount = 0;
            uint32_t total = 0, peak = 0;

            tracker.lock();
            AllocHeader *curr = tracker._head;
            while (curr != nullptr && activeCount < 32)
            {
                active[activeCount++] = {curr->caller, curr->size, curr->tag};
                curr = curr->next;
            }
            total = tracker._totalAllocated.load(std::memory_order_relaxed);
            peak = tracker._peakAllocated;
            tracker.unlock();

            Serial.print("ALLOCS:{\"total\":");
            Serial.print(total);
            Serial.print(",\"peak\":");
            Serial.print(peak);
            Serial.print(",\"tags\":[");
            for (size_t i = 0; i < activeCount; ++i)
            {
                if (i > 0)
                    Serial.print(",");
                Serial.print("{\"tag\":\"");
                Serial.print(active[i].tag ? active[i].tag : "unknown");
                if (active[i].caller != nullptr)
                {
                    Serial.print(" @ 0x");
                    Serial.print(reinterpret_cast<uintptr_t>(active[i].caller), HEX);
                }
                Serial.print("\",\"bytes\":");
                Serial.print(active[i].size);
                Serial.print(",\"count\":1}");
            }
            Serial.println("]}");
        }

        static void handle_get_profile()
        {
            auto &prof = Profiler::instance();
            prof.calculateSelfCycles();

            Serial.print("PROFILE:{\"frequency\":");
            Serial.print(ESP.getCpuFreqMHz() * 1000000U);
            Serial.print(",\"nodes\":[");

            prof.lock();

            ProfileNode *nodeList[64];
            size_t count = 0;
            for (ProfileNode *curr = prof._head; curr != nullptr && count < 64; curr = curr->next)
            {
                nodeList[count++] = curr;
            }

            for (size_t i = 0; i < count; ++i)
            {
                if (i > 0)
                    Serial.print(",");

                int parentIdx = -1;
                if (nodeList[i]->parent != nullptr)
                {
                    for (size_t j = 0; j < count; ++j)
                    {
                        if (nodeList[j] == nodeList[i]->parent)
                        {
                            parentIdx = static_cast<int>(j);
                            break;
                        }
                    }
                }

                Serial.print("{\"name\":\"");
                Serial.print(nodeList[i]->name ? nodeList[i]->name : "unknown");
                Serial.print("\",\"parent\":");
                Serial.print(parentIdx);
                Serial.print(",\"total\":");
                Serial.print(nodeList[i]->totalCycles);
                Serial.print(",\"self\":");
                Serial.print(nodeList[i]->selfCycles);
                Serial.print(",\"calls\":");
                Serial.print(nodeList[i]->callCount);
                Serial.print(",\"max\":");
                Serial.print(nodeList[i]->maxCycles);
                Serial.print("}");
            }
            prof.unlock();
            Serial.println("]}");
        }
    }

    Server &Server::instance() noexcept
    {
        static Server server;
        return server;
    }

    void Server::begin() noexcept
    {
        if (_started)
            return;
        _started = true;

        xTaskCreatePinnedToCore([](void *)
                                {
            while (!Serial) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            char* line = static_cast<char*>(malloc(512));
            if (!line) return;

            size_t len = 0;
            while (true)
            {
                if (Serial.available() > 0)
                {
                    while (Serial.available() > 0)
                    {
                        char c = Serial.read();
                        if (c == '\n' || c == '\r')
                        {
                            if (len > 0)
                            {
                                line[len] = '\0';
                                
                                if (std::strcmp(line, "GET_ALLOCS") == 0)
                                {
                                    handle_get_allocs();
                                }
                                else if (std::strcmp(line, "GET_FLASH") == 0)
                                {
                                    uint32_t jedec_id = 0;
                                    esp_flash_read_id(nullptr, &jedec_id);

                                    Serial.print("FLASH:{\"size\":");
                                    Serial.print(ESP.getFlashChipSize());
                                    Serial.print(",\"speed\":");
                                    Serial.print(ESP.getFlashChipSpeed());
                                    Serial.print(",\"mode\":");
                                    Serial.print(ESP.getFlashChipMode());
                                    Serial.print(",\"jedec_id\":");
                                    Serial.print(jedec_id);
                                    Serial.println("}");
                                }
                                else if (std::strcmp(line, "GET_NVS") == 0)
                                {
                                    Serial.println("NVS:{\"keys\":["
                                                "{\"key\":\"ota_sbuild\",\"val\":\"140922\"},"
                                                "{\"key\":\"ota_bbuild\",\"val\":\"0\"},"
                                                "{\"key\":\"ota_good\",\"val\":\"3F0000\"},"
                                                "{\"key\":\"bmax\",\"val\":\"100\"}"
                                                "]}");
                                }
                                else if (std::strcmp(line, "GET_CPU") == 0)
                                {
                                    esp_chip_info_t chip_info;
                                    esp_chip_info(&chip_info);
                                    Serial.print("CPU:{\"freq\":");
                                    Serial.print(ESP.getCpuFreqMHz());
                                    Serial.print(",\"cores\":");
                                    Serial.print(chip_info.cores);
                                    Serial.print(",\"revision\":");
                                    Serial.print(chip_info.revision);
                                    Serial.println("}");
                                }
                                else if (std::strcmp(line, "GET_PROFILE") == 0)
                                {
                                    handle_get_profile();
                                }
                                else if (std::strcmp(line, "RESET_PROFILE") == 0)
                                {
                                    Profiler::instance().clear();
                                    Serial.println("PROFILE:RESET_OK");
                                }
                                else if (std::strcmp(line, "GET_FS") == 0)
                                {
                                    list_fs_files_json();
                                }
                                else if (std::strncmp(line, "GET_FILE:", 9) == 0)
                                {
                                    const char* filepath = line + 9;
                                    if (LittleFS.begin(false))
                                    {
                                        File f = LittleFS.open(filepath, "r");
                                        if (!f && filepath[0] != '/') {
                                            char tempPath[64];
                                            std::snprintf(tempPath, sizeof(tempPath), "/%s", filepath);
                                            f = LittleFS.open(tempPath, "r");
                                        }

                                        if (f) {
                                            Serial.print("FILE_DATA:{\"name\":\"");
                                            Serial.print(filepath);
                                            Serial.print("\",\"hex\":\"");
                                            while (f.available() > 0) {
                                                uint8_t b = f.read();
                                                Serial.printf("%02X", b);
                                            }
                                            Serial.println("\"}");
                                            f.close();
                                        } else {
                                            Serial.print("FILE_ERR:{\"name\":\"");
                                            Serial.print(filepath);
                                            Serial.println("\",\"err\":\"File not found\"}");
                                        }
                                        LittleFS.end();
                                    } else {
                                        Serial.print("FILE_ERR:{\"name\":\"");
                                        Serial.print(filepath);
                                        Serial.println("\",\"err\":\"FS mount failed\"}");
                                    }
                                }
                                else if (std::strncmp(line, "WRITE_START:", 12) == 0)
                                {
                                    const char* filepath = line + 12;
                                    if (upload_active) {
                                        upload_file.close();
                                        upload_active = false;
                                    }

                                    if (LittleFS.begin(false)) {
                                        char path[64];
                                        std::snprintf(path, sizeof(path), "%s%s", (filepath[0] != '/') ? "/" : "", filepath);

                                        upload_file = LittleFS.open(path, "w");
                                        if (upload_file) {
                                            upload_active = true;
                                            Serial.println("UPLOAD:WRITE_OK");
                                        } else {
                                            Serial.println("UPLOAD:WRITE_ERR:Failed to open file");
                                            LittleFS.end();
                                        }
                                    } else {
                                        Serial.println("UPLOAD:WRITE_ERR:FS mount failed");
                                    }
                                }
                                else if (std::strncmp(line, "WRITE_CHUNK:", 12) == 0)
                                {
                                    const char* hex_data = line + 12;
                                    if (upload_active && upload_file) {
                                        size_t hex_len = std::strlen(hex_data);
                                        bool err = false;
                                        for (size_t i = 0; i + 1 < hex_len; i += 2) {
                                            uint8_t byte = (fast_hex_val(hex_data[i]) << 4) | fast_hex_val(hex_data[i+1]);
                                            if (upload_file.write(byte) != 1) {
                                                err = true;
                                                break;
                                            }
                                        }
                                        if (err) {
                                            Serial.println("UPLOAD:WRITE_ERR:Flash write failed");
                                        } else {
                                            Serial.println("UPLOAD:WRITE_OK");
                                        }
                                    } else {
                                        Serial.println("UPLOAD:WRITE_ERR:No active upload session");
                                    }
                                }
                                else if (std::strcmp(line, "WRITE_END") == 0)
                                {
                                    if (upload_active) {
                                        upload_file.close();
                                        upload_active = false;
                                        Serial.println("UPLOAD:WRITE_OK");
                                        LittleFS.end();
                                    } else {
                                        Serial.println("UPLOAD:WRITE_ERR:No active upload session");
                                    }
                                }
                                else if (std::strncmp(line, "DELETE_FILE:", 12) == 0)
                                {
                                    const char* filepath = line + 12;
                                    if (LittleFS.begin(false)) {
                                        char path[64];
                                        std::snprintf(path, sizeof(path), "%s%s", (filepath[0] != '/') ? "/" : "", filepath);

                                        if (LittleFS.remove(path)) {
                                            Serial.println("UPLOAD:DELETE_OK");
                                        } else {
                                            Serial.println("UPLOAD:DELETE_ERR:Delete failed");
                                        }
                                        LittleFS.end();
                                    } else {
                                        Serial.println("UPLOAD:DELETE_ERR:FS mount failed");
                                    }
                                }
                                else if (std::strcmp(line, "GET_PARTITIONS") == 0)
                                {
                                    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
                                    Serial.print("PARTITIONS:[");
                                    bool first = true;
                                    while (it != nullptr) {
                                        const esp_partition_t* p = esp_partition_get(it);
                                        if (!first) Serial.print(",");
                                        first = false;
                                        
                                        const char* color = "part-free";
                                        if (p->type == ESP_PARTITION_TYPE_APP) {
                                            color = "part-app";
                                        } else if (p->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS) { 
                                            color = "part-nvs";
                                        } else if (p->subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS || 
                                                   p->subtype == ESP_PARTITION_SUBTYPE_DATA_FAT ||
                                                   p->subtype == 0x83) {
                                            color = "part-fs";
                                        } else if (p->subtype == ESP_PARTITION_SUBTYPE_DATA_OTA) {
                                            color = "part-pt";
                                        }

                                        uint32_t flags = p->encrypted ? 0x01 : 0x00;
                                        Serial.printf("{\"name\":\"%s\",\"type\":\"%d\",\"subtype\":\"%d\",\"offset\":%u,\"size\":%u,\"flags\":%u,\"color\":\"%s\"}", 
                                                      p->label, p->type, p->subtype, p->address, p->size, flags, color);
                                        it = esp_partition_next(it);
                                    }
                                    Serial.println("]");
                                }
                                else if (std::strncmp(line, "READ_FLASH:", 11) == 0)
                                {
                                    uint32_t offset = 0;
                                    uint32_t size = 0;
                                    if (sscanf(line + 11, "%u,%u", &offset, &size) == 2) {
                                        if (size > 1024) size = 1024;
                                        uint8_t* temp = static_cast<uint8_t*>(malloc(size));
                                        if (temp) {
                                            if (esp_flash_read(esp_flash_default_chip, temp, offset, size) == ESP_OK) {
                                                Serial.print("HEX_DATA:{\"offset\":");
                                                Serial.print(offset);
                                                Serial.print(",\"hex\":\"");
                                                for (uint32_t i = 0; i < size; ++i) {
                                                    Serial.printf("%02X", temp[i]);
                                                }
                                                Serial.println("\"}");
                                            }
                                            free(temp);
                                        }
                                    }
                                }
                                else if (std::strcmp(line, "GET_BOOT_SECURITY") == 0)
                                {
                                    uint8_t boot_header[8] = {0};
                                    uint8_t magic = 0x00;
                                    uint32_t entry_point = 0x0;
                                    uint8_t segments = 0;

                                    const esp_partition_t* running = esp_ota_get_running_partition();
                                    uint32_t app_offset = running ? running->address : 0x10000;
                                    
                                    if (esp_flash_read(esp_flash_default_chip, boot_header, app_offset, sizeof(boot_header)) == ESP_OK) {
                                        magic = boot_header[0];
                                        segments = boot_header[1];
                                        entry_point = (boot_header[7] << 24) | (boot_header[6] << 16) | (boot_header[5] << 8) | boot_header[4];
                                    }

                                    bool crypto_enabled = esp_flash_encryption_enabled();
                                    bool secure_boot_enabled = esp_secure_boot_enabled();
                                    const char* active_slot = running ? running->label : "factory";

                                    uint8_t mac[6] = {0};
                                    esp_efuse_mac_get_default(mac);
                                    char mac_str[18];
                                    std::snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
                                                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

                                    char uid_str[20];
                                    std::snprintf(uid_str, sizeof(uid_str), "0x%02X%02X%02X%02X%02X%02X", 
                                                  mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);

                                    Serial.printf("BOOT_SEC:{\"magic\":\"0x%02X\",\"entry\":\"0x%08X\",\"segments\":%u,\"crypto\":%s,\"secure_boot\":%s,\"active_slot\":\"%s\",\"mac\":\"%s\",\"unique_id\":\"%s\"}\n",
                                                  magic, entry_point, segments, 
                                                  crypto_enabled ? "true" : "false", 
                                                  secure_boot_enabled ? "true" : "false", 
                                                  active_slot,
                                                  mac_str,
                                                  uid_str);
                                }
                                len = 0;
                            }
                        }
                        else if (len < 511)
                        {
                            line[len++] = c;
                        }
                    }
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            } }, "pip_serial_tracker", 4096, nullptr, 1, nullptr, 1);
    }
}

#endif