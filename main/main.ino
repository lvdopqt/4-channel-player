#include "FS.h"
#include "SD_MMC.h"
#include "driver/i2s.h"

// ==========================================
// PIN CONFIG
// ==========================================

// I2S 0 (DAC 1)
#define I2S0_BCLK 27
#define I2S0_LRCK 26
#define I2S0_DOUT 25

// I2S 1 (DAC 2)
#define I2S1_BCLK 32
#define I2S1_LRCK 33
#define I2S1_DOUT 22

// FILE NAME IN SD CARD (CONSTANT NAME)
#define AUDIO_FILE "/music.wav"

// BUFFER
#define BUFFER_SIZE 8192

// Buffers on RAM
uint8_t sd_buffer[BUFFER_SIZE];
uint8_t i2s0_buffer[BUFFER_SIZE];
uint8_t i2s1_buffer[BUFFER_SIZE];

File audioFile;
uint32_t data_offset = 44; // WAV STANDARD
uint32_t sample_rate = 44100;
uint16_t num_channels = 2;

// ==========================================
// I2S FUNCTIONS
// ==========================================

void inicializar_i2s(i2s_port_t port_num, int bck, int lrck, int dout, uint32_t s_rate) {
    Serial.printf("[I2S] Initializing port %d with %d Hz...\n", port_num, s_rate);
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = s_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = bck,
        .ws_io_num = lrck,
        .data_out_num = dout,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(port_num, &i2s_config, 0, NULL);
    i2s_set_pin(port_num, &pin_config);
    i2s_zero_dma_buffer(port_num);
    i2s_start(port_num);
}

// ==========================================
// AUDIO FUNCTIONS (Reading WAV Header)
// ==========================================

bool parse_wav_header(File &file) {
    if (file.size() < 44) {
        Serial.println("[WAV] ERRO: FILE TOO SMALL TO BE A WAV.");
        return false;
    }

    uint8_t header[44];
    file.read(header, 44);

    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F') {
        Serial.println("[WAV] ERRO: NOT A VALID RIFF/WAV FILE.");
        return false;
    }

    num_channels = header[22] | (header[23] << 8);
    sample_rate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
    uint16_t bits_per_sample = header[34] | (header[35] << 8);

    Serial.println("====== FILE DETAILS ======");
    Serial.printf("(Sample Rate): %d Hz\n", sample_rate);
    Serial.printf("Channels: %d\n", num_channels);
    Serial.printf("Bits per Sample: %d\n", bits_per_sample);
    Serial.println("=================================");

    if (bits_per_sample != 16) {
        Serial.println("[WAV] ERRO: The file must be 16-bits!");
        return false;
    }
    
    if (num_channels != 2 && num_channels != 4) {
        Serial.println("[WAV] ERRO: Only 2 channels (Stereo) or 4 channels are supported.");
        return false;
    }

    // Search for "data" block
    data_offset = 12;
    while (data_offset < file.size()) {
        file.seek(data_offset);
        uint8_t chunk_id[4];
        file.read(chunk_id, 4);
        
        uint32_t chunk_size = 0;
        file.read((uint8_t*)&chunk_size, 4);

        if (chunk_id[0] == 'd' && chunk_id[1] == 'a' && chunk_id[2] == 't' && chunk_id[3] == 'a') {
            data_offset += 8;
            Serial.printf("[WAV] Audio data begins at the byte: %d\n", data_offset);
            break;
        }
        data_offset += 8 + chunk_size;
    }

    return true;
}

// ==========================================
// SETUP
// ==========================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- INITIALIZING DUA DAC SYSTEM ---");

    // Mounting SDMMC (1-Bit)
    Serial.println("[SDMMC] Attempting to mount the SD card...");
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("[SDMMC] CRITICAL: Mounting failed! Verify the wires and connections.");
        while(1) delay(100);
    }
    Serial.println("[SDMMC] Success mounting the SD card!");

    // Open Audio File
    Serial.printf("[SDMMC] Searching for %s...\n", AUDIO_FILE);
    audioFile = SD_MMC.open(AUDIO_FILE);
    if (!audioFile) {
        Serial.println("[SDMMC] CRITICAL: File not found! Rename your song to 'music.wav'");
        while(1) delay(100);
    }

    // Process WAV
    if (!parse_wav_header(audioFile)) {
        while(1) delay(100); // Locks if not a valid wav
    }

    // Initializing I2S based on sample rate
    inicializar_i2s(I2S_NUM_0, I2S0_BCLK, I2S0_LRCK, I2S0_DOUT, sample_rate);
    delay(150);
    inicializar_i2s(I2S_NUM_1, I2S1_BCLK, I2S1_LRCK, I2S1_DOUT, sample_rate);

    // Positioning the file cursor in the music data, skiping the headers
    audioFile.seek(data_offset);
    Serial.println("\n[PLAYER] Tudo pronto! Iniciando reproducao em LOOP...");
}

// ==========================================
// MAIN LOOP (Reading and Playing)
// ==========================================

void loop() {
    // Reads a chunk of the file
    size_t bytes_read = audioFile.read(sd_buffer, BUFFER_SIZE);

    // Is it the end of the file? Reestart the loop!
    if (bytes_read == 0) {
        Serial.println("[PLAYER] Fim do arquivo. Reiniciando a faixa...");
        audioFile.seek(data_offset);
        return;
    }

    size_t out_bytes_0 = 0;
    size_t out_bytes_1 = 0;

    // ==============================================================
    // Channel Splitter (THE SAUCE)
    // ==============================================================
    if (num_channels == 4) {
        // 4 channel file:
        // Each frame has 8 bytes [L1][R1][L2][R2]
        size_t quadros = bytes_read / 8;
        for (size_t i = 0; i < quadros; i++) {
            // DAC 1 receives tracks 1 and 2 (L1, R1)
            i2s0_buffer[i*4 + 0] = sd_buffer[i*8 + 0];
            i2s0_buffer[i*4 + 1] = sd_buffer[i*8 + 1];
            i2s0_buffer[i*4 + 2] = sd_buffer[i*8 + 2];
            i2s0_buffer[i*4 + 3] = sd_buffer[i*8 + 3];

            // DAC 2 receives tracks 3 and 4 (L2, R2)
            i2s1_buffer[i*4 + 0] = sd_buffer[i*8 + 4];
            i2s1_buffer[i*4 + 1] = sd_buffer[i*8 + 5];
            i2s1_buffer[i*4 + 2] = sd_buffer[i*8 + 6];
            i2s1_buffer[i*4 + 3] = sd_buffer[i*8 + 7];
        }
        out_bytes_0 = quadros * 4;
        out_bytes_1 = quadros * 4;
    } 
    else if (num_channels == 2) {
        // Stereo (2 Channels):
        // Copy the audio 1:1 so both DACs will play the same thing
        memcpy(i2s0_buffer, sd_buffer, bytes_read);
        memcpy(i2s1_buffer, sd_buffer, bytes_read);
        out_bytes_0 = bytes_read;
        out_bytes_1 = bytes_read;
    }

    // ==============================================================
    // SEND TO DACS
    // ==============================================================
    size_t bytes_escritos_0;
    size_t bytes_escritos_1;
    
    // Send to DAC 1 (Wait for DMA buffer to be clean)
    i2s_write(I2S_NUM_0, i2s0_buffer, out_bytes_0, &bytes_escritos_0, portMAX_DELAY);
    
    // Send to DAC 2
    i2s_write(I2S_NUM_1, i2s1_buffer, out_bytes_1, &bytes_escritos_1, portMAX_DELAY);
}