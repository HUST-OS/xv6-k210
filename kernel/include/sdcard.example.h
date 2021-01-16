//
// Created by lumin on 2020/11/2.
//

#ifndef LAB8_SDCARD_H
#define LAB8_SDCARD_H

#include <defs.h>

/*
 * SD Card Commands
 * @brief  Commands: CMDxx = CMD-number | 0x40
 */
#define SD_CMD_SIGN (0x40)
#define SD_CMD0     0   // chip reset
#define SD_CMD8     8   // voltage negotiation
#define SD_CMD9     9   // read CSD register
#define SD_CMD10    10  // read CID register
#define SD_CMD12    12  // end multiple continuous sector read
#define SD_CMD17    17  // start single sector read
#define SD_CMD18    18  // start multiple continuous sector read and send start sector
#define SD_ACMD23   23  // start multiple continuous sector write and send sector count
#define SD_CMD24    24  // start single sector write
#define SD_CMD25    25  // start multiple continuous sector write and send start sector
#define SD_ACMD41   41  // capacity mode set
#define SD_CMD55    55  // ACMD prefix
#define SD_CMD58    58  // read CCS(card capacity status)

#define SD_INIT_MODE_RESULT_OK                      (0x01)
#define SD_TRANS_MODE_RESULT_OK                     (0x00)
#define SD_START_DATA_READ_RESPONSE                 (0xFE)
#define SD_START_DATA_SINGLE_BLOCK_WRITE_TOKEN      (0xFE)
#define SD_START_DATA_MULTIPLE_BLOCK_WRITE_TOKEN    (0xFC)

/**
 * CMD frame format
 * |   CRC  | arg[31:24] | arg[23:16] | arg[15:8] | arg[7:0] |   CMD  |
 * | byte 5 |    bit 4   |    bit 3   |   bit 2   |   bit 1  |  bit 0 |
 */
#define SD_CMD_CMD_BIT              0
#define SD_CMD_ARG_MSB0             1
#define SD_CMD_ARG_MSB1             2
#define SD_CMD_ARG_MSB2             3
#define SD_CMD_ARG_MSB3             4
#define SD_CMD_CRC_BIT              5
#define SD_CMD_FRAME_SIZE           6

#define SD_EMPTY_FILL               (0xFF)

#define SD_R3_RESPONSE_REST_LENGTH  4
#define SD_R7_RESPONSE_REST_LENGTH  4

// SD card here uses SPI mode, pin
// names also inherit from it.
typedef struct
{
    int mosi_pin;   // pin num of Master Out Slave In
    int miso_pin;   // pin num of Master In Slave Out
    int sclk_pin;   // pin num of SPI Clock
    int cs_pin;     // pin num of Chip Selector
    int cs_gpio_num;// what is this..?
} sdcard_hardware_pin_config_t;

typedef struct
{
    uint8_t CSDStruct;           /*!< CSD structure */
    uint8_t SysSpecVersion;      /*!< System specification version */
    uint8_t Reserved1;           /*!< Reserved */
    uint8_t TAAC;                /*!< Data read access-time 1 */
    uint8_t NSAC;                /*!< Data read access-time 2 in CLK cycles */
    uint8_t MaxBusClkFrec;       /*!< Max. bus clock frequency */
    uint16_t CardComdClasses;    /*!< Card command classes */
    uint8_t RdBlockLen;          /*!< Max. read data block length */
    uint8_t PartBlockRead;       /*!< Partial blocks for read allowed */
    uint8_t WrBlockMisalign;     /*!< Write block misalignment */
    uint8_t RdBlockMisalign;     /*!< Read block misalignment */
    uint8_t DSRImpl;             /*!< DSR implemented */
    uint8_t Reserved2;           /*!< Reserved */
    uint32_t DeviceSize;         /*!< Device Size */
    uint8_t MaxRdCurrentVDDMin;  /*!< Max. read current @ VDD min */
    uint8_t MaxRdCurrentVDDMax;  /*!< Max. read current @ VDD max */
    uint8_t MaxWrCurrentVDDMin;  /*!< Max. write current @ VDD min */
    uint8_t MaxWrCurrentVDDMax;  /*!< Max. write current @ VDD max */
    uint8_t DeviceSizeMul;       /*!< Device size multiplier */
    uint8_t EraseGrSize;         /*!< Erase group size */
    uint8_t EraseGrMul;          /*!< Erase group size multiplier */
    uint8_t WrProtectGrSize;     /*!< Write protect group size */
    uint8_t WrProtectGrEnable;   /*!< Write protect group enable */
    uint8_t ManDeflECC;          /*!< Manufacturer default ECC */
    uint8_t WrSpeedFact;         /*!< Write speed factor */
    uint8_t MaxWrBlockLen;       /*!< Max. write data block length */
    uint8_t WriteBlockPaPartial; /*!< Partial blocks for write allowed */
    uint8_t Reserved3;           /*!< Reserded */
    uint8_t ContentProtectAppli; /*!< Content protection application */
    uint8_t FileFormatGrouop;    /*!< File format group */
    uint8_t CopyFlag;            /*!< Copy flag (OTP) */
    uint8_t PermWrProtect;       /*!< Permanent write protection */
    uint8_t TempWrProtect;       /*!< Temporary write protection */
    uint8_t FileFormat;          /*!< File Format */
    uint8_t ECC;                 /*!< ECC code */
    uint8_t CSD_CRC;             /*!< CSD CRC */
    uint8_t Reserved4;           /*!< always 1*/
    uint8_t CSizeMlut;           /*!<  */
} SD_CSD;

/**
* @brief  Card Identification Data: CID Register
*/
typedef struct
{
    uint8_t ManufacturerID; /*!< ManufacturerID */
    uint16_t OEM_AppliID;   /*!< OEM/Application ID */
    uint32_t ProdName1;     /*!< Product Name part1 */
    uint8_t ProdName2;      /*!< Product Name part2*/
    uint8_t ProdRev;        /*!< Product Revision */
    uint32_t ProdSN;        /*!< Product Serial Number */
    uint8_t Reserved1;      /*!< Reserved1 */
    uint16_t ManufactDate;  /*!< Manufacturing Date */
    uint8_t CID_CRC;        /*!< CID CRC */
    uint8_t Reserved2;      /*!< always 1 */
} SD_CID;

/**
* @brief SD Card information
*/
typedef struct
{
    SD_CSD SD_csd;
    SD_CID SD_cid;
    uint64_t CardCapacity;  /*!< Card Capacity */
    uint32_t CardBlockSize; /*!< Card Block Size */
    uint8_t active;
} SD_CardInfo;

extern SD_CardInfo cardinfo;

/**
 * Initialize the SD Card
 */
void sd_init();

/**
 * Synchronously read single/multiple sector(s) from SD card
 *
 * @param data_buff pointer to the buffer that receives the data read from the SD.
 * @param sector SD's internal address to read from.
 * @param count count of sectors to be read
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_read_sector(uint8_t *data_buff, uint32_t sector, uint32_t count);

/**
 * Synchronously write single/multiple sector(s) from SD card
 *
 * @param data_buff pointer to the buffer to be written to the SD.
 * @param sector SD's internal address to write to.
 * @param count count of sectors to be read
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
uint8_t sd_write_sector(uint8_t *data_buff, uint32_t sector, uint32_t count);

#endif //LAB8_SDCARD_H
