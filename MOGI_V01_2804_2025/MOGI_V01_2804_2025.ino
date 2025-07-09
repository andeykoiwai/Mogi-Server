/*
Mogi Robot - Main Control Program
===============================

Features:
- TFT Animation with clock and date display
- Voice capabilities 
- WiFi configuration system
- Configurable robot name
- MP3 file upload support
- Mogi configuration options:
  * Name
  * Serial number
  * Eye parameters (size, blink, breath)
  * Colors and dimensions
- update serial number ask for quiz
- Quiz support (math and English)
- Update Sprite agar tidak fliker
- update pesan baru dari server
- update pesan ke teman
- masih bug pada saat mendelet pesan, karena pasti di kirim lagi oleh server
Server URLs:
- Cloud: http://andeykoiwai.pythonanywhere.com  
- Local: http://192.168.0.100:8888

Last Updated: 2025
*/

#include <WiFi.h>
#include <WebServer.h>
#include "WebConfig.h"
#include "Microphone.h"
#include "FileHandler.h"

//#----- calling mogi ------
#define EIDSP_QUANTIZE_FILTERBANK   0
#include <MOGIONLINE_inferencing.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "driver/i2s.h"
//#----- end calling mogi --

//#----- animasi baru (mata robot)----------
#include <TFT_eSPI.h>
#include "mata_robot_gc9a01.h"

TFT_eSPI display = TFT_eSPI();
roboEyes eyes;

//#----- end animasi-------
//#----- Speker ----------
#include "Audio.h"
#include <driver/i2s.h>
#include "FS.h"
#include "FFat.h"
//#----memory cek------
#include "esp_heap_caps.h"
bool animasidansuara = false;
void printHeapInfo() {
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);
}
//#--------------------
#define I2S_DOUT 4
#define I2S_BCLK 5
#define I2S_LRC 6

Audio audio;
String endplaying = "";
//#----- END Speker ----------

//#----- set calling mogi struktur -----
/** Audio buffers, pointers and selectors */
typedef struct {
  int16_t *buffer;
  uint8_t buf_ready;
  uint32_t buf_count;
  uint32_t n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool record_status = true;
bool callmogi = true;
//#----- end calling mogi struktur -----

//#-----------standar setting -------
#define BUTTON_PIN 7
WebConfig internet_config("mogi");
Microphone mic;
FileHandler hendelfile;
String tulisan ="";
bool endplayingbool = true;
bool textanimasi = true;
bool newMessage = false;
//#--------- end standar setting -----

void setup() {
  Serial.begin(115200);
  ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
  internet_config.begin();
  MogiConfig config = internet_config.getMogiConfig();
  hendelfile.setMogiConfig(config);
  //MogiConfig config = internet_config.getMogiConfig();
  //#-------- Speker ---------
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(30);
  //#-------- EnD ----------
  
  //#-------- animasi baru ------
  display.init();
  display.setRotation(0);
  display.fillScreen(TFT_BLACK);
  eyes.begin(240, 240, 100);  // Ukuran layar 240x240, 40 FPS 75 |10
  eyes.open();
  eyes.setMainColor(config.eyeColor);
  eyes.setWidth(config.eyeWidth, config.eyeWidth);
  eyes.setHeight(config.eyeHeight, config.eyeHeight);
  eyes.setBorderradius(config.borderRadius, config.borderRadius);
  eyes.setSpacebetween(config.spaceBetween);
  eyes.setMood(DEFAULT);
  eyes.setPosition(DEFAULT);
  eyes.setAutoblinker(config.autoBlinker, config.autoBlinkerTime, config.autoBlinkerVariation);
  eyes.setBreathing(config.breathing, config.breathingSpeed, config.breathingAmount);
  xTaskCreate(eyeAnimationTask, "Eye Animation Task", 4096, NULL, 2, NULL);
  if(WiFi.localIP().toString()!="0.0.0.0"){
    if (internet_config.cekStatusWifi()) {
      eyes.resyncTime();
      eyes.setTimeDisplay(true);
    } else {
      eyes.setTimeDisplay(false);
    }
    eyes.setIdleMode(true, 5, 3);     // Gerakan idle setiap 5±3 detik
    eyes.setText("Panggil Saya Mogi!");
    if(FFat.exists("/nama.mp3")){
      playMusic("nama.mp3");
      while(endplaying!="nama.mp3"){
        audio.loop();
      }
      if(endplaying=="nama.mp3"){
        delay(1000);
        // eyes.setText("- Mogi! -");
        eyes.setText(internet_config.getMogiConfig().name, internet_config.getMogiConfig().textColor);
        xTaskCreate(callingMogi, "Mogi Inference Task", 4096 , NULL, 1, NULL);
        animasidansuara = true;
        pinMode(BUTTON_PIN, INPUT_PULLUP);
      }
    }
    else{
      delay(1000);
      eyes.setText(" Masuk "+WiFi.localIP().toString()+" di Browser / mogi.local dan upload mp3 file");
    }
    
  }
  else{
    // deteksi awalan belum terinstall
    eyes.setPosition(N);
    eyes.setText("untuk pertamakali cari wifi mogi, setelah konek ke wifi mogi, masuk ke browser 192.168.4.1 jika terkoneksi IP "+ WiFi.localIP().toString() +" upload file di browser dengan mengetik ip ini");
    
  }
  
}

void loop() {
  audio.loop();
  internet_config.handleClient();

  commandConsol();

  if(!callmogi){
    komunikasi_ESP_Server();
  }

  if(!endplayingbool){
    if(endplaying=="output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3"){
      eyes.setMood(DEFAULT);
      endplayingbool = true;
    }
  }
  
  // Tambahkan pengecekan pesan baru
  checkNewMessages();
}

void textAnimasi(String text, uint16_t color){
  if(textanimasi && !newMessage){
    eyes.setText(text, color);
  }
}
// Task untuk animasi mata
void eyeAnimationTask(void *pvParameters) {
  while (1) {
    eyes.update();

    static unsigned long lastMoodChange = 0;
    if (millis() - lastMoodChange > 50000) { // Ganti mood setiap 10 detik
      lastMoodChange = millis();
      int mood = random(4); // Pilih mood acak
      
      switch(mood) {
        case 0:
          eyes.setMood(DEFAULT);
          textAnimasi(internet_config.getMogiConfig().name, internet_config.getMogiConfig().textColor);
          break;
        case 1:
          eyes.setMood(TIRED);
          if(animasidansuara){
            playMusic("sedih.mp3");
            textAnimasi("Hiks", internet_config.getMogiConfig().textColor);
          }
          break;
        case 2:
          eyes.setMood(ANGRY);
          if(animasidansuara){
            playMusic("marah.mp3");
            textAnimasi("hemm", internet_config.getMogiConfig().textColor);
          }
          break;
        case 3:
          eyes.setMood(HAPPY);
          if(animasidansuara){
            playMusic("geli.mp3");
            textAnimasi("Geli", internet_config.getMogiConfig().textColor);
          }
          break;
      }
      
      // Trigger animasi acak
      if(random(100) > 70) {
        if(random(2)) {
          eyes.anim_confused();
          if(animasidansuara){
            playMusic("kaget.mp3");
          }
        } else {
          eyes.anim_laugh();
          if(animasidansuara){
            playMusic("geli.mp3");
          }
        }
      }
    }
    
    vTaskDelay(20 / portTICK_PERIOD_MS); // Delay untuk mengontrol FPS
  }
}

// command consol buat download dll / test
void commandConsol(){
  if(Serial.available()){   
    tulisan = Serial.readStringUntil('\n');
    tulisan.trim();
    if(tulisan == "data"){
      internet_config.getFileList();
    }
    else if(tulisan == "rekam"){
      callmogi = false;
      record_status = false;
      delay(100);
      delay(100);
      // microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false;
      delay(100);
      mic.startRecording();
    }
    else if(tulisan == "cekwifi"){
      internet_config.cekStatusWifi();
    }
    else if(tulisan == "cekserver"){
      hendelfile.ServerStatus(internet_config.getServerUrl());
    }
    else if(tulisan == "upload"){
      hendelfile.upload("recording.wav",internet_config.getServerUrl());
    }
    else if(tulisan == "esp_user"){
      Serial.println(hendelfile.getesp_user());
    }
    else if(tulisan == "server_user"){
      Serial.println(hendelfile.getserver_user());
    }
    else if(tulisan == "download") {
        // Gunakan serial number untuk nama file
        String outputFile = "output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3";
        hendelfile.download(outputFile, internet_config.getServerUrl());
    }
    else if(tulisan == "downloadall") {
        callmogi = false;
        record_status = false;
        eyes.setAutoblinker(false);
        eyes.setIdleMode(false);
        eyes.setText("Sedang Mendownload");
        delay(200);
        
        // Array of files to download
        const char* files[] = {
            "nama.mp3",
            "noserver.mp3", 
            "nowifi.mp3",
            "rekam.mp3",
            "upload.mp3",
            "download.mp3",
            "error.mp3",
            "geli.mp3",
            "kaget.mp3",
            "bosen.mp3",
            "sedih.mp3",
            "marah.mp3"
        };
        
        // Download each file
        for(const char* file : files) {
            hendelfile.download(file, internet_config.getServerUrl());
            delay(200);
            eyes.setText("Download " + String(file));
        }
        
        eyes.setText("Download Selesai");
        Serial.println("Download Selesai");
        delay(1000);
        ESP.restart();
        xTaskCreate(callingMogi, "Mogi Inference Task", 4096, NULL, 2, NULL);
    }
    else{
      Serial.println("Perintah tidak dikenali. ^^ .");
    }
  }
}

/*
membuat logic speker plasy speker
*/
void playMusic(String namamusik) {
    String namamusik_ = namamusik;    
    if (FFat.exists("/" + namamusik_)) {
        audio.stopSong(); 
        Serial.println("playing. "+ namamusik_);
        audio.connecttoFS(FFat, ("/" + namamusik_).c_str());
    } else {
        Serial.println("tidak menemukan filemusik.");
    }
}


// Add this updated komunikasi_ESP_Server function to the client code

// Updated komunikasi_ESP_Server function with English learning support
  void komunikasi_ESP_Server(){
    
    ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
    if(internet_config.cekStatusWifi()){
      if(hendelfile.ServerStatus(internet_config.getServerUrl())){
        //cara memanggil suara di taks wajib seperti ini
        textanimasi = false;
        callmogi = false;
        record_status = false;
        animasidansuara = false;
        eyes.setMood(HAPPY);
        eyes.anim_laugh();
        eyes.setText("Mendengarkan... ", TFT_GREEN);
        playMusic("rekam.mp3");
        while(endplaying!="rekam.mp3"){
          audio.loop();
        }
        // sampai sini
        if(endplaying=="rekam.mp3"){
          // ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
          mic.startRecording();
          
        }
        if(!mic.isRecordingActive() && endplaying=="rekam.mp3"){
          playMusic("upload.mp3");
          eyes.setMood(TIRED);
          eyes.anim_confused();
          eyes.setText("Memproses...", TFT_YELLOW);
          // ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
          while(endplaying!="upload.mp3"){
            // menunggu menjadi upload
            audio.loop();
          }
          if(endplaying=="upload.mp3"){
            eyes.setMood(DEFAULT);
            hendelfile.upload("recording.wav",internet_config.getServerUrl());
            if(hendelfile.responweb!=200){
              playMusic("error.mp3");
              while(endplaying!="error.mp3"){
                audio.loop();
              }
              eyes.setMood(ANGRY);
              Serial.println("upload error");
              return;
            }
            // mencoba merapihkan dari upload
            else if(hendelfile.responweb==200){
              if(!hendelfile.isUpload() && endplaying=="upload.mp3"){
            
                playMusic("download.mp3");
                while(endplaying!="download.mp3"){
                  // menunggu menjadi download
                  audio.loop();
                }
                if(endplaying=="download.mp3"){
                  eyes.setMood(TIRED);
                  eyes.anim_confused();
                  
                  // Use serial number for output file
                  String outputFile = "output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3";
                  hendelfile.download(outputFile, internet_config.getServerUrl());
                  
                  if(!hendelfile.isDownload() && endplaying=="download.mp3"){
                    // ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
                    eyes.setMood(HAPPY);
                    eyes.anim_laugh();
                    eyes.setPosition(N);
                    eyes.setIdleMode(false);
                    
                    // Check if we're in a quiz session
                    if(hendelfile.isInQuiz()){
                      // Set different colors based on quiz type
                      uint16_t quizColor;
                      if(hendelfile.getQuizType() == "math") {
                        quizColor = TFT_CYAN;
                      } else if(hendelfile.getQuizType() == "english") {
                        quizColor = TFT_MAGENTA;
                      } else {
                        quizColor = TFT_WHITE;
                      }
                      
                      eyes.setText("Quiz: " + hendelfile.getserver_user(), quizColor);
                      playMusic("output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3");
                      
                      while(endplaying!="output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3"){
                        audio.loop();
                      }
                      
                      // If we've finished the quiz, return to normal mode
                      if(hendelfile.getserver_user().indexOf("Kuis selesai!") >= 0 || 
                         hendelfile.getserver_user().indexOf("Quiz completed!") >= 0) {
                        if(endplaying=="output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3"){
                          xTaskCreate(callingMogi, "Mogi Inference Task", 4096 , NULL, 2, NULL);
                          eyes.clearText();
                          eyes.setIdleMode(true);
                          endplayingbool = false;
                          Serial.println("kembali memanggil mogi");
                          animasidansuara = false;
                        }
                      } else {
                        // Continue with next quiz question
                        // After playing the question, immediately start listening for answer
                        if(endplaying=="output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3"){
                          delay(500); // Short pause before recording
                          
                          // Set text based on quiz type
                          if(hendelfile.getQuizType() == "math") {
                            eyes.setText("Jawab pertanyaan...", TFT_GREEN);
                          } else if(hendelfile.getQuizType() == "english") {
                            eyes.setText("Answer the question...", TFT_GREEN);
                          }
                          
                          // Recursive call to handle next question
                          komunikasi_ESP_Server();
                        }
                      }
                    } else {
                      // Normal conversation mode
                      eyes.setText(hendelfile.getserver_user(), TFT_WHITE);
                      playMusic("output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3");
                      
                      while(endplaying!="output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3"){
                        audio.loop();
                      }
                      
                      if(endplaying=="output_" + String(internet_config.getMogiConfig().serialNumber) + ".mp3"){
                        xTaskCreate(callingMogi, "Mogi Inference Task", 4096 , NULL, 2, NULL);
                        eyes.clearText();
                        eyes.setIdleMode(true);
                        endplayingbool = false;
                        Serial.println("kembali memanggil mogi");
                        animasidansuara = false;
                        textanimasi = true;
                      }
                    }
                  }
                }
                else{
                  playMusic("error.mp3");
                  while(endplaying!="error.mp3"){
                    audio.loop();
                  }
                  //currentGifIndex = 6;
                  eyes.setMood(ANGRY);
                  eyes.anim_confused();
                  eyes.setText("komunikasi terputus..", TFT_RED);
                  Serial.println("download error");
                }
              }
            }
            else{
              playMusic("error.mp3");
              while(endplaying!="error.mp3"){
                audio.loop();
              }
              eyes.setMood(ANGRY);
              eyes.anim_confused();
              eyes.setText("komunikasi terputus..", TFT_RED);
              Serial.println("upload error");
            }
          }
        }
      }
      else{
        Serial.println("tidak terhubung ke server");  
        playMusic("noserver.mp3");
        eyes.setMood(TIRED);
        eyes.setText("tidak terhubung ke server", TFT_RED);
        while(endplaying!="noserver.mp3"){
          audio.loop();
        }
        xTaskCreate(callingMogi, "Mogi Inference Task", 4096 , NULL, 2, NULL);
      }
    }
    else{
      Serial.println("Wifi tidak terhubung");
      eyes.setMood(TIRED);
      eyes.anim_confused();
      eyes.setText("Wifi tidak terhubung", TFT_RED);
      playMusic("nowifi.mp3");
      while(endplaying!="nowifi.mp3"){
        audio.loop();
      }
      xTaskCreate(callingMogi, "Mogi Inference Task", 4096 , NULL, 2, NULL);
    }
    Serial.print("Tanya Jawab ");
    Serial.println("Done .");
    ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
  }

// untuk mengecek bahwa audio benar di play
void audio_eof_mp3(const char *info){  //end of file
  endplaying = info;
  Serial.print("end mp3 ");
  Serial.println(endplaying);
}
/* AWAL CALLING MOGI*/
void setcallMogi(){
  // Serial.println("Edge Impulse Inferencing Demo");

  // // Summary of inferencing settings
  ei_printf("Inferencing settings:\n");
  ei_printf("\tInterval: ");
  ei_printf_float((float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf(" ms.\n");
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

  ei_printf("\nStarting continuous inference in 2 seconds...\n");

  if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
      ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
      return;
  }

  ei_printf("Recording...\n");
}

void callingMogi(void *pvParameters){
  record_status = true;
  callmogi = true;
  setcallMogi();
  while(callmogi){
    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

  // Hanya tampilkan prediksi untuk "mogi"
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      //test bug
      ei_printf("    %s: ", result.classification[ix].label);
      ei_printf_float(result.classification[ix].value);
      ei_printf("\n");
      if (strcmp(result.classification[ix].label, "mogi") == 0){
        if(result.classification[ix].value > 0.7){
          // vTaskDelay(500 / portTICK_PERIOD_MS);
          callmogi = false;
          record_status = false;
          vTaskDelay(200);
          vTaskDelete(NULL);
        }
      }
    }
  }
  if(!callmogi){
    callmogi = false;
    record_status = false;
    vTaskDelay(200);
    vTaskDelete(NULL);
  }
}

static void audio_inference_callback(uint32_t n_bytes){
  for(int i = 0; i < n_bytes>>1; i++) {
      inference.buffer[inference.buf_count++] = sampleBuffer[i];

      if(inference.buf_count >= inference.n_samples) {
        inference.buf_count = 0;
        inference.buf_ready = 1;
      }
  }
}

static void capture_samples(void* arg) {
  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read = i2s_bytes_to_read;

  while (record_status) {
      // read data at once from i2s
      i2s_read((i2s_port_t)1, (void*)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);

      if (bytes_read <= 0) {
          ei_printf("Error in I2S read : %d", bytes_read);
      }
      else {
          if (bytes_read < i2s_bytes_to_read) {
              ei_printf("Partial I2S read");
          }

          // Scale the data (otherwise the sound is too quiet)
          for (int x = 0; x < i2s_bytes_to_read/2; x++) {
              sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
          }

          if (record_status) {
              audio_inference_callback(i2s_bytes_to_read);
          }
          else {
              break;
          }
      }
  }
  vTaskDelete(NULL);
}

static bool microphone_inference_start(uint32_t n_samples){
  inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

  if(inference.buffer == NULL) {
      return false;
  }

  inference.buf_count  = 0;
  inference.n_samples  = n_samples;
  inference.buf_ready  = 0;

  ei_sleep(100);

  record_status = true;

  xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL);

  return true;
}


static bool microphone_inference_record(void){
  bool ret = true;

  while (inference.buf_ready == 0) {
      delay(10);
  }

  inference.buf_ready = 0;
  return ret;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr){
  numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
  return 0;
}

// Fungsi untuk mengecek pesan baru
void checkNewMessages() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 30000) { // Cek setiap 30 detik
    lastCheck = millis();
    
    // Cek pesan baru dari server
    internet_config.checkNewMessagesFromServer();
    
    // Dapatkan jumlah pesan yang belum dibaca
    int unreadCount = 0;
    for (int i = 0; i < internet_config.getMessageCount(); i++) {
      if (!internet_config.getMessage(i).isRead) {
        unreadCount++;
      }
    }
    
    if (unreadCount > 0) {
      eyes.setMood(HAPPY);
      eyes.setText("Ada " + String(unreadCount) + " pesan baru!", TFT_GREEN);
      playMusic("upload.mp3");
      newMessage = true;
      // delay(2000);
      //eyes.clearText();
    }else{
      newMessage = false;
    }
  }
}
