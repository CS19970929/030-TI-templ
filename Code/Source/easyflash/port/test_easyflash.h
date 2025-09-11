#ifndef _test_easyflash_h
#define _test_easyflash_h

typedef struct {
    uint16_t voltage;
    int16_t temperature;
    uint8_t mos_state;
} BmsStatus;

typedef struct {
    uint32_t timestamp;
    uint8_t event_id;
    uint8_t level;
    uint8_t cell_index;
    uint64_t value;
} __attribute__((packed)) BmsLogEntry;



#define LOG_MAX_COUNT 50

void test_env(void);
void get_soc_easyflash(void);
void test_soc_flash_logi(void);
void test_easyflash_app(void);

#endif