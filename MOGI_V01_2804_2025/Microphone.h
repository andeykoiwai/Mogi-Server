#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <FS.h>
#include <FFat.h>
#include <driver/i2s.h>

#define SAMPLE_RATE (16000)  // Frekuensi sampling
#define SAMPLE_BITS (16)     // Bit per sampel
#define WAV_HEADER_SIZE (44) // Ukuran header WAV
#define I2S_PORT I2S_NUM_1
#define I2S_WS 42   // Pin untuk I2S Word Select
#define I2S_SD 2    // Pin untuk I2S Data
#define I2S_SCK 41  // Pin untuk I2S Clock
#define RECORD_TIME (8)
#define I2S_CHANNEL_NUM (1)
#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * SAMPLE_RATE * SAMPLE_BITS / 8 * RECORD_TIME)

class Microphone {
private:
    File file;           // File untuk menyimpan rekaman
    bool isRecording;    // Status perekaman

    void i2sInit();                  // Fungsi untuk inisialisasi I2S
    
public:
    int waktubicara = 6;
    void setwaktubicara(int waktu);
    Microphone();                    // Konstruktor
    void startRecording();           // Mulai perekaman
    void stopRecording();            // Stop perekaman
    bool isRecordingActive();        // Mengecek status perekaman
    void writeWavHeader(byte *header, int wavSize);           // Fungsi untuk menulis header WAV ke file
    void i2s_adc_data_scale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len);
};

#endif
