#include "Microphone.h"
#include <Arduino.h>

// Konstruktor Microphone
Microphone::Microphone() {
    isRecording = false;
    i2sInit();
}

// Inisialisasi I2S
void Microphone::i2sInit() {
//    i2s_driver_uninstall(I2S_PORT);
    i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),          // Mode RX untuk menerima data
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(SAMPLE_BITS),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,    // Satu kanal (Mono)
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
        .intr_alloc_flags = 0,
        .dma_buf_count = 10,
        .dma_buf_len = 1024,
        .use_apll = false
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

    // Mengatur pin I2S
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,  // Tidak digunakan karena hanya menerima data
        .data_in_num = I2S_SD
    };

    i2s_set_pin(I2S_PORT, &pin_config);
}

// Menulis header WAV
void Microphone::writeWavHeader(byte *header, int wavSize) {
    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    unsigned int fileSize = wavSize + 44 - 8;
    header[4] = (byte)(fileSize & 0xFF);
    header[5] = (byte)((fileSize >> 8) & 0xFF);
    header[6] = (byte)((fileSize >> 16) & 0xFF);
    header[7] = (byte)((fileSize >> 24) & 0xFF);
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    header[16] = 0x10;
    header[17] = 0x00;
    header[18] = 0x00;
    header[19] = 0x00;
    header[20] = 0x01;
    header[21] = 0x00;
    header[22] = 0x01;
    header[23] = 0x00;
    header[24] = 0x80;
    header[25] = 0x3E;
    header[26] = 0x00;
    header[27] = 0x00;
    header[28] = 0x00;
    header[29] = 0x7D;
    header[30] = 0x01;
    header[31] = 0x00;
    header[32] = 0x02;
    header[33] = 0x00;
    header[34] = 0x10;
    header[35] = 0x00;
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    header[40] = (byte)(wavSize & 0xFF);
    header[41] = (byte)((wavSize >> 8) & 0xFF);
    header[42] = (byte)((wavSize >> 16) & 0xFF);
    header[43] = (byte)((wavSize >> 24) & 0xFF);
}

// Fungsi untuk mulai merekam
void Microphone::startRecording() {
    isRecording = true;
    FFat.remove("/recording.wav");
    Serial.println("data berhasil dibersihkan.");
    vTaskDelay(50 / portTICK_PERIOD_MS);
    // Buka file untuk rekaman
    file = FFat.open("/recording.wav", FILE_WRITE);
    if (!file) {
        Serial.println("Gagal membuka file!");
        return;
    }
    // Menulis header WAV
    byte header[44];
    writeWavHeader(header, FLASH_RECORD_SIZE);
    file.write(header, 44);
    Serial.println("WAV Header selesai ditulis.");

    int i2s_read_len = 4 * 1024; // 16 * 1024 normal
    size_t bytes_read;
    int flash_wr_size = 0;
    char *i2s_read_buff = (char *)calloc(i2s_read_len, sizeof(char));
    uint8_t *flash_write_buff = (uint8_t *)calloc(i2s_read_len, sizeof(char));
    i2s_read(I2S_PORT, (void *)i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
    Serial.println(" *** Recording Start *** ");
    unsigned long lastSoundTime = millis();
    while(flash_wr_size < FLASH_RECORD_SIZE){
        i2s_read(I2S_PORT, (void *)i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
        i2s_adc_data_scale(flash_write_buff, (uint8_t *)i2s_read_buff, i2s_read_len);
        file.write((const byte *)flash_write_buff, i2s_read_len);
        flash_wr_size += i2s_read_len;
        ets_printf("Sound recording %u%%\n", flash_wr_size * 100 / FLASH_RECORD_SIZE);
        ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
        if (millis() - lastSoundTime >= (waktubicara*1000)) { 
            Serial.println("record sudah 6 detik.");
            break;
        }
    }

    // Menutup file setelah selesai
    file.close();
    free(i2s_read_buff);
    i2s_read_buff = NULL;
    free(flash_write_buff);
    flash_write_buff = NULL;
    isRecording = false;
    Serial.println("Berhasil di simpan.");
}

// Fungsi untuk menghentikan perekaman
void Microphone::stopRecording() {
    isRecording = false;
}

// Fungsi untuk mengecek apakah perekaman aktif
bool Microphone::isRecordingActive() {
    return isRecording;
}
void Microphone::setwaktubicara(int waktu){
    waktubicara = waktu;
}
void Microphone::i2s_adc_data_scale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len) {
  uint32_t j = 0;
  uint32_t dac_value = 0;
  for (int i = 0; i < len; i += 2) {
    dac_value = ((((uint16_t)(s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
    d_buff[j++] = 0;
    d_buff[j++] = dac_value * 256 / 2048;
  }
}
