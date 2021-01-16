//
// Created by lumin on 2020/11/2.
//
#include <sdcard.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <spi.h>
#include <stdio.h>
#include <string.h>

#define SECTOR_SIZE             (512)

#define SD_SPI_CLK              (200000UL)
#define SD_SPI_HIGH_SPEED_CLK   (25000000UL)
#define SD_SPI_DEVICE           SPI_DEVICE_1
#define SD_SPI_WORK_MODE        SPI_WORK_MODE_0
#define SD_SPI_FRAME_FORMAT     SPI_FF_STANDARD
#define SD_SPI_DATA_BIT_LENGTH  8
#define SD_SPI_ENDIAN           0
#define SD_SPI_CHIP_SELECT      SPI_CHIP_SELECT_1

#define SD_CMD0_ARGS            (0)
#define SD_CMD0_CRC             (0x95)
#define SD_CMD55_ARGS           (0)
#define SD_CMD55_CRC            (0)
#define SD_ACMD41_CRC           (0)
#define SD_CMD58_ARGS           (0)
#define SD_CMD58_CRC            (1)
#define SD_CMD10_ARGS           (0)
#define SD_CMD10_CRC            (0)
#define SD_VOLTAGE_SELECT       (0x0100)
#define SD_CHECK_PATTERN        (0xAA)
#define SD_CMD8_CRC             (0x87)
#define SD_HIGH_CAPACITY        (0x40000000)
#define SD_ERROR_RES            (0xFF)
#define SD_TIMEMOUT_COUNT       (0xFF)

static void sdcard_pin_mapping_init();
static void sdcard_gpiohs_init();
static void sdcard_spi_init();
static void sd_SPI_mode_prepare();
static void SD_CS_HIGH();
static void SD_CS_LOW();
static void SD_HIGH_SPEED_ENABLE();
static void sd_write_data(uint8_t *frame, uint32_t length);
static void sd_read_data(uint8_t *frame, uint32_t length);
static void sd_send_cmd(uint8_t cmd, uint32_t args, uint8_t crc);
static void sd_end_cmd();
static uint8_t sd_get_R1_response();
static void sd_get_R3_rest_response(uint8_t *frame);
static void sd_get_R7_rest_response(uint8_t *frame);
static uint8_t sd_get_data_write_response();
static int sd_switch_to_SPI_mode();
static int sd_negotiate_voltage();
static int sd_set_SDXC_capacity();
static int sd_query_capacity_status();
static int sd_get_cardinfo(SD_CardInfo *cardinfo);
static int sd_get_csdregister(SD_CSD *SD_csd);
static int sd_get_cidregister(SD_CID *SD_cid);
static void sd_test();

sdcard_hardware_pin_config_t config = {
        28, 26, 27, 29, 29
};

SD_CardInfo cardinfo;

void sd_init()
{
    cardinfo.active = 0;

    // Part 1: Hardware infrastructure configuration
    sdcard_pin_mapping_init();
    sdcard_gpiohs_init();
    sdcard_spi_init();

    // Part 2: SD protocol(SPI Mode)
    sd_SPI_mode_prepare();

    if(sd_switch_to_SPI_mode() != 0)
        return;
    if(sd_negotiate_voltage() != 0)
        return;
    if(sd_set_SDXC_capacity() != 0)
        return;
    if(sd_query_capacity_status() != 0)
        return;

    SD_HIGH_SPEED_ENABLE();

    if(sd_get_cardinfo(&cardinfo) != 0)
        return;

    // Part 3: Read/Write test
//    sd_test();
    cprintf("Successfully initialize SD card!\n");
}

uint8_t sd_read_sector(uint8_t *data_buff, uint32_t sector, uint32_t count)
{
    uint8_t dummy_crc[2], multiple;
    if (count == 1){
        multiple = 0;
        sd_send_cmd(SD_CMD17, sector, 0);
    } else{
        multiple = 1;
        sd_send_cmd(SD_CMD18, sector, 0);
    }

    uint32_t result = sd_get_R1_response();
    if (result != SD_TRANS_MODE_RESULT_OK) {
        cprintf("Read sector(s) CMD error! result = %d\n", result);
        return SD_ERROR_RES;
    }

    while (count)
    {
        result = sd_get_R1_response();
        if (result != SD_START_DATA_READ_RESPONSE) {
            cprintf("Data read response error! result = %d\n", result);
            return SD_ERROR_RES;
        }

        sd_read_data(data_buff, SECTOR_SIZE);
        sd_read_data(dummy_crc, 2);

        data_buff += SECTOR_SIZE;
        count--;
    }

    sd_end_cmd();
    if (multiple){
        sd_send_cmd(SD_CMD12, 0, 0);
        sd_get_R1_response();
        sd_end_cmd();
        sd_end_cmd();
    }

    if (count > 0) {
        cprintf("Not all sectors are read!\n");
        return SD_ERROR_RES;
    }

    return 0;
}

uint8_t sd_write_sector(uint8_t *data_buff, uint32_t sector, uint32_t count)
{
    assert(((uint32_t)data_buff) % 4 == 0);
    uint8_t token[2] = {0xFF, 0x00}, dummpy_crc[2] = {0xFF, 0xFF};

    if (count == 1){
        token[1] = SD_START_DATA_SINGLE_BLOCK_WRITE_TOKEN;
        sd_send_cmd(SD_CMD24, sector, 0);
    } else{
        token[1] = SD_START_DATA_MULTIPLE_BLOCK_WRITE_TOKEN;
        sd_send_cmd(SD_ACMD23, count, 0);
        sd_get_R1_response();
        sd_end_cmd();
        sd_send_cmd(SD_CMD25, sector, 0);
    }

    if (sd_get_R1_response() != SD_TRANS_MODE_RESULT_OK) {
        cprintf("Write sector(s) CMD error!\n");
        return SD_ERROR_RES;
    }

        while (count)
    {
        sd_write_data(token, 2);
        sd_write_data(data_buff, SECTOR_SIZE);
        sd_write_data(dummpy_crc, 2);

        if (sd_get_data_write_response() != SD_TRANS_MODE_RESULT_OK) {
            cprintf("Data write response error!\n");
            return SD_ERROR_RES;
        }

        data_buff += SECTOR_SIZE;
        count--;
    }

    sd_end_cmd();
    sd_end_cmd();

    if (count > 0) {
        cprintf("Not all sectors are written!\n");
        return SD_ERROR_RES;
    }

    return 0;
}

static void sdcard_pin_mapping_init()
{
    fpioa_set_function(config.sclk_pin, FUNC_SPI1_SCLK);
    fpioa_set_function(config.mosi_pin, FUNC_SPI1_D0);
    fpioa_set_function(config.miso_pin, FUNC_SPI1_D1);
    fpioa_set_function(config.cs_pin, FUNC_GPIOHS0 + config.cs_gpio_num);
}

static void sdcard_gpiohs_init()
{
    gpiohs_set_drive_mode(config.cs_gpio_num, GPIO_DM_OUTPUT);
}

static void sdcard_spi_init()
{
    spi_set_clk_rate(SD_SPI_DEVICE, SD_SPI_CLK);
}

static void sd_SPI_mode_prepare()
{
    /*!< Send dummy byte 0xFF, 10 times with CS high */
    /*!< Rise CS and MOSI for 80 clocks cycles */
    /*!< Send dummy byte 0xFF */
    uint8_t frame[10];
    for (int i = 0; i < 10; i++)
        frame[i] = SD_EMPTY_FILL;
    /*!< SD chip select high */
    SD_CS_HIGH();
    sd_write_data(frame, 10);
}

static void SD_CS_HIGH()
{
    gpiohs_set_pin_output_value(config.cs_gpio_num, GPIO_PV_HIGH);
}

static void SD_CS_LOW()
{
    gpiohs_set_pin_output_value(config.cs_gpio_num, GPIO_PV_LOW);
}

static void SD_HIGH_SPEED_ENABLE()
{
    spi_set_clk_rate(SD_SPI_DEVICE, SD_SPI_HIGH_SPEED_CLK);
}

static void sd_write_data(uint8_t *frame, uint32_t length)
{
    spi_init(SD_SPI_DEVICE, SD_SPI_WORK_MODE,
             SD_SPI_FRAME_FORMAT, SD_SPI_DATA_BIT_LENGTH, SD_SPI_ENDIAN);
    spi_send_data_standard(SD_SPI_DEVICE, SD_SPI_CHIP_SELECT, frame, length);
}

static void sd_read_data(uint8_t *frame, uint32_t length)
{
    spi_init(SD_SPI_DEVICE, SD_SPI_WORK_MODE,
             SD_SPI_FRAME_FORMAT, SD_SPI_DATA_BIT_LENGTH, SD_SPI_ENDIAN);
    spi_receive_data_standard(SD_SPI_DEVICE, SD_SPI_CHIP_SELECT, frame, length);
}

static void sd_send_cmd(uint8_t cmd, uint32_t args, uint8_t crc)
{
    // prepare cmd frame
    uint8_t frame[SD_CMD_FRAME_SIZE];
    frame[SD_CMD_CMD_BIT] = (SD_CMD_SIGN | cmd);
    frame[SD_CMD_ARG_MSB0] = (uint8_t)(args >> 24);
    frame[SD_CMD_ARG_MSB1] = (uint8_t)(args >> 16);
    frame[SD_CMD_ARG_MSB2] = (uint8_t)(args >> 8);
    frame[SD_CMD_ARG_MSB3] = (uint8_t)args;
    frame[SD_CMD_CRC_BIT] = crc;

    // real send operation
    SD_CS_LOW();
    sd_write_data(frame,SD_CMD_FRAME_SIZE);
}

static void sd_end_cmd()
{
    uint8_t fill = SD_EMPTY_FILL;
    SD_CS_HIGH();
    sd_write_data(&fill, 1);
}

static uint8_t sd_get_R1_response()
{
    uint8_t result;

    uint16_t timeout = SD_TIMEMOUT_COUNT;
    while (timeout--)
    {
        sd_read_data(&result, 1);
        if (result != SD_EMPTY_FILL)
            return result;
    }
    return SD_ERROR_RES;
}

static void sd_get_R3_rest_response(uint8_t *frame)
{
    sd_read_data(frame, SD_R3_RESPONSE_REST_LENGTH);
}

static void sd_get_R7_rest_response(uint8_t *frame)
{
    sd_read_data(frame, SD_R7_RESPONSE_REST_LENGTH);
}

static uint8_t sd_get_data_write_response()
{
    uint8_t result;
    sd_read_data(&result, 1);

    // protocol defined correct response
    if ((result & 0x1F) != 0x05)
        return SD_ERROR_RES;

    do {
        sd_read_data(&result, 1);
    } while (result == SD_TRANS_MODE_RESULT_OK);

    return SD_TRANS_MODE_RESULT_OK;
}

static int sd_switch_to_SPI_mode()
{
    uint8_t result;
    sd_send_cmd(SD_CMD0, SD_CMD0_ARGS, SD_CMD0_CRC);
    result = sd_get_R1_response();
    sd_end_cmd();

    if (result != SD_INIT_MODE_RESULT_OK){
        cprintf("Fail to connect to SD card!\n");
        return SD_ERROR_RES;
    }
    return 0;
}

static int sd_negotiate_voltage()
{
    uint8_t frame[SD_R7_RESPONSE_REST_LENGTH],result;
    sd_send_cmd(SD_CMD8, SD_VOLTAGE_SELECT | SD_CHECK_PATTERN, SD_CMD8_CRC);
    result = sd_get_R1_response();
    sd_get_R7_rest_response(frame);
    sd_end_cmd();

    if (result != SD_INIT_MODE_RESULT_OK){
        cprintf("Fail to negotiate voltage with SD card!\n");
        return SD_ERROR_RES;
    }
    return 0;
}

static int sd_set_SDXC_capacity()
{
    uint8_t result;

    uint16_t timeout = SD_TIMEMOUT_COUNT;
    while (timeout--)
    {
        sd_send_cmd(SD_CMD55, SD_CMD55_ARGS, SD_CMD55_CRC);
        result = sd_get_R1_response();
        sd_end_cmd();
        if (result != SD_INIT_MODE_RESULT_OK){
            cprintf("Fail to prepare application command when set SDXC capacity!\n");
            return SD_ERROR_RES;
        }

        sd_send_cmd(SD_ACMD41, SD_HIGH_CAPACITY, SD_ACMD41_CRC);
        result = sd_get_R1_response();
        sd_end_cmd();
        if (result == SD_TRANS_MODE_RESULT_OK)
            return 0;
    }
    cprintf("Timeout to set card capacity!\n");
    return SD_ERROR_RES;
}

static int sd_query_capacity_status()
{
    uint8_t frame[SD_R3_RESPONSE_REST_LENGTH], result;

    uint16_t timeout = SD_TIMEMOUT_COUNT;
    while (timeout--)
    {
        sd_send_cmd(SD_CMD58, SD_CMD58_ARGS, SD_CMD58_CRC);
        result = sd_get_R1_response();
        sd_get_R3_rest_response(frame);
        sd_end_cmd();
        if (result == SD_TRANS_MODE_RESULT_OK)
            break;
    }
    if (timeout == 0) {
        cprintf("Timeout to query card capacity status!\n");
        return SD_ERROR_RES;
    }
    // protocol defined correct response format
    if ((frame[0] & 0x40) == 0) {
        cprintf("SDXC card capacity status wrong!\n");
        return SD_ERROR_RES;
    }
    return 0;
}

static int sd_get_cardinfo(SD_CardInfo *cardinfo)
{
    if(sd_get_csdregister(&cardinfo->SD_csd) != 0)
        return SD_ERROR_RES;
    if(sd_get_cidregister(&cardinfo->SD_cid) != 0)
        return SD_ERROR_RES;

    cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1) * 2 * SECTOR_SIZE;
    cardinfo->CardBlockSize = (1 << (cardinfo->SD_csd.RdBlockLen));
    cardinfo->CardCapacity *= cardinfo->CardBlockSize;
    cardinfo->active = 1;

    return 0;
}

static int sd_get_csdregister(SD_CSD *SD_csd)
{
    uint8_t csd_frame[18];

    // protocol
    sd_send_cmd(SD_CMD9, 0, 0);
    if (sd_get_R1_response() != SD_TRANS_MODE_RESULT_OK) {
        cprintf("Get CSD cmd error!\n");
        return SD_ERROR_RES;
    }
    if (sd_get_R1_response() != SD_START_DATA_READ_RESPONSE) {
        cprintf("Get CSD data response error!\n");
        return SD_ERROR_RES;
    }

    // retrieve data, 16 for SD_CID, 2 for CRC
    sd_read_data(csd_frame, 18);
    sd_end_cmd();

    // set field
    SD_csd->CSDStruct = csd_frame[0] >> 6;
    SD_csd->SysSpecVersion = (csd_frame[0] & 0x3C) >> 2;
    SD_csd->Reserved1 = csd_frame[0] & 0x03;

    SD_csd->TAAC = csd_frame[1];

    SD_csd->NSAC = csd_frame[2];

    SD_csd->MaxBusClkFrec = csd_frame[3];

    SD_csd->CardComdClasses = (csd_frame[4] << 4);

    SD_csd->CardComdClasses |= (csd_frame[5] >> 4);
    SD_csd->RdBlockLen = csd_frame[5] & 0x0F;

    SD_csd->PartBlockRead = csd_frame[6] >> 7;
    SD_csd->WrBlockMisalign = (csd_frame[6] & 0x40) >> 6;
    SD_csd->RdBlockMisalign = (csd_frame[6] & 0x20) >> 5;
    SD_csd->DSRImpl = (csd_frame[6] & 0x10) >> 4;
    SD_csd->Reserved2 = 0;

    SD_csd->DeviceSize = (csd_frame[7] & 0x3F) << 16;
    SD_csd->DeviceSize |= csd_frame[8] << 8;
    SD_csd->DeviceSize |= csd_frame[9];

    SD_csd->EraseGrSize = (csd_frame[10] & 0x40) >> 6;
    SD_csd->EraseGrMul = ((csd_frame[10] & 0x3F) << 1);

    SD_csd->EraseGrMul |= (csd_frame[11] >> 7);
    SD_csd->WrProtectGrSize = csd_frame[11] & 0x7F;

    SD_csd->WrProtectGrEnable = csd_frame[12] >> 7;
    SD_csd->ManDeflECC = (csd_frame[12] & 0x60) >> 5;
    SD_csd->WrSpeedFact = (csd_frame[12] & 0x1C) >> 2;
    SD_csd->MaxWrBlockLen = ((csd_frame[12] & 0x03) << 2);

    SD_csd->MaxWrBlockLen |= (csd_frame[13] >> 6);
    SD_csd->WriteBlockPaPartial = (csd_frame[13] & 0x20) >> 5;
    SD_csd->Reserved3 = 0;
    SD_csd->ContentProtectAppli = csd_frame[13] & 0x01;

    SD_csd->FileFormatGrouop = csd_frame[14] >> 7;
    SD_csd->CopyFlag = (csd_frame[14] & 0x40) >> 6;
    SD_csd->PermWrProtect = (csd_frame[14] & 0x20) >> 5;
    SD_csd->TempWrProtect = (csd_frame[14] & 0x10) >> 4;
    SD_csd->FileFormat = (csd_frame[14] & 0x0C) >> 2;
    SD_csd->ECC = csd_frame[14] & 0x03;

    SD_csd->CSD_CRC = csd_frame[15] >> 1;
    SD_csd->Reserved4 = 1;

    return 0;
}

static int sd_get_cidregister(SD_CID *SD_cid) {
    uint8_t cid_frame[18];

    // protocol
    sd_send_cmd(SD_CMD10, SD_CMD10_ARGS, SD_CMD10_CRC);
    if (sd_get_R1_response() != SD_TRANS_MODE_RESULT_OK) {
        cprintf("Get CID cmd error!\n");
        return SD_ERROR_RES;
    }
    if (sd_get_R1_response() != SD_START_DATA_READ_RESPONSE) {
        cprintf("Get CID data response error!\n");
        return SD_ERROR_RES;
    }

    // retrieve data, 16 for SD_CID, 2 for CRC
    sd_read_data(cid_frame, 18);
    sd_end_cmd();

    // set field
    SD_cid->ManufacturerID = cid_frame[0];
    SD_cid->OEM_AppliID = (cid_frame[1] << 8);
    SD_cid->OEM_AppliID |= cid_frame[2];
    SD_cid->ProdName1 = (cid_frame[3] << 24);
    SD_cid->ProdName1 |= (cid_frame[4] << 16);
    SD_cid->ProdName1 |= (cid_frame[5] << 8);
    SD_cid->ProdName1 |= cid_frame[6];
    SD_cid->ProdName2 = cid_frame[7];
    SD_cid->ProdRev = cid_frame[8];
    SD_cid->ProdSN = (cid_frame[9] << 24);
    SD_cid->ProdSN |= (cid_frame[10] << 16);
    SD_cid->ProdSN |= (cid_frame[11] << 8);
    SD_cid->ProdSN |= cid_frame[12];
    SD_cid->Reserved1 = cid_frame[13] >> 4;
    SD_cid->ManufactDate = ((cid_frame[13] & 0x0F) << 8);
    SD_cid->ManufactDate |= cid_frame[14];
    SD_cid->CID_CRC = cid_frame[15] >> 1;
    SD_cid->Reserved2 = 1;

    return 0;
}

static void sd_test()
{
    uint64_t num_sectors = cardinfo.CardCapacity / SECTOR_SIZE;
    uint32_t sector = num_sectors - 10;

    uint8_t buffer0[2 * SECTOR_SIZE], buffer1[2 * SECTOR_SIZE], buffer2[2 * SECTOR_SIZE], buffer3[2 * SECTOR_SIZE];
    const char *msg = "Well! I've often seen a cat without a grin', thought Alice, 'but a grin without a cat! It's the most curious thing I ever saw in my life!'";

    cprintf("Start SD card read/write test\n", sector);

    memset(buffer0, 0, SECTOR_SIZE);
    memset(buffer1, 0, SECTOR_SIZE);
    memset(buffer2, 0, SECTOR_SIZE);
    memset(buffer3, 0, SECTOR_SIZE);
    cprintf("- single sector read/write test on sector 0x%x\n", sector);
    assert(sd_read_sector(buffer0, sector, 1) == 0);
    cprintf("\tread sector 0x%x successfully!\n", sector);

    memcpy(buffer1, msg, strlen(msg) + 1 );
    assert(memcmp(buffer1, buffer2, SECTOR_SIZE) != 0);
    assert(sd_write_sector(buffer1, sector, 1) == 0);
    cprintf("\twrite sector 0x%x successfully!\n", sector);

    assert(sd_read_sector(buffer2, sector, 1) == 0);
    assert(memcmp(buffer1, buffer2, SECTOR_SIZE) == 0);
    cprintf("\treread sector 0x%x successfully!\n", sector);

    assert(sd_write_sector(buffer0, sector, 1) == 0);
    assert(sd_read_sector(buffer3, sector, 1) == 0);
    assert(memcmp(buffer0, buffer3, SECTOR_SIZE) == 0);
    cprintf("\trestore sector 0x%x successfully!\n", sector);

    memset(buffer0, 0, 2 * SECTOR_SIZE);
    memset(buffer1, 0, 2 * SECTOR_SIZE);
    memset(buffer2, 0, 2 * SECTOR_SIZE);
    memset(buffer3, 0, 2 * SECTOR_SIZE);
    cprintf("- multiple sector read/write test on sector 0x%x\n", sector);
    assert(sd_read_sector(buffer0, sector, 2) == 0);
    cprintf("\tread sector 0x%x successfully!\n", sector);

    memcpy(buffer1, msg, strlen(msg) + 1 );
    assert(memcmp(buffer1, buffer2, 2 * SECTOR_SIZE) != 0);
    assert(sd_write_sector(buffer1, sector, 2) == 0);
    cprintf("\twrite sector 0x%x successfully!\n", sector);

    assert(sd_read_sector(buffer2, sector, 2) == 0);
    assert(memcmp(buffer1, buffer2, 2 * SECTOR_SIZE) == 0);
    cprintf("\treread sector 0x%x successfully!\n", sector);

    assert(sd_write_sector(buffer0, sector, 2) == 0);
    assert(sd_read_sector(buffer3, sector, 2) == 0);
    assert(memcmp(buffer0, buffer3, 2 * SECTOR_SIZE) == 0);
    cprintf("\trestore sector 0x%x successfully!\n", sector);

    cprintf("SD card initialized successfully!\n");
}