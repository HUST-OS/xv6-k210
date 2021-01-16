//
// Created by lumin on 2020/11/2.
//

#ifndef LAB8_SPI_H
#define LAB8_SPI_H

#include <defs.h>
#include <io.h>

#define SPI01_WORK_MODE_OFFSET              6
#define SPI012_TRANSFER_MODE_OFFSET         8
#define SPI01_DATA_BIT_LENGTH_OFFSET        16
#define SPI01_FRAME_FORMAT_OFFSET           21

#define SPI3_WORK_MODE_OFFSET               8
#define SPI3_TRANSFER_MODE_OFFSET           10
#define SPI3_DATA_BIT_LENGTH_OFFSET         0
#define SPI3_FRAME_FORMAT_OFFSET            22

#define SPI_DATA_BIT_LENGTH_BIT             5
#define SPI_MIN_DATA_BIT_LENGTH             4
#define SPI_MAX_DATA_BIT_LENGTH             (1 << SPI_DATA_BIT_LENGTH_BIT)

#define SPI_BAUDRATE_DEFAULT_VAL            (0x14)
#define SPI_INTERRUPT_DISABLE               (0x00)
#define SPI_DMACR_DEFAULT_VAL               (0x00)
#define SPI_DMATDLR_DEFAULT_VAL             (0x00)
#define SPI_DMARDLR_DEFAULT_VAL             (0x00)
#define SPI_SLAVE_DISABLE                   (0x00)
#define SPI_MASTER_DISABLE                  (0x00)
#define SPI_MASTER_ENABLE                   (0x01)
#define SPI_TMOD_DEFAULT_VAL                0

#define SPI_FIFO_CAPCITY_IN_BYTE            (32)

typedef struct
{
    /* SPI Control Register 0                                    (0x00)*/
    volatile uint32_t ctrlr0;
    /* SPI Control Register 1                                    (0x04)*/
    volatile uint32_t ctrlr1;
    /* SPI Enable Register                                       (0x08)*/
    volatile uint32_t ssienr;
    /* SPI Microwire Control Register                            (0x0c)*/
    volatile uint32_t mwcr;
    /* SPI Slave Enable Register                                 (0x10)*/
    volatile uint32_t ser;
    /* SPI Baud Rate Select                                      (0x14)*/
    volatile uint32_t baudr;
    /* SPI Transmit FIFO Threshold Level                         (0x18)*/
    volatile uint32_t txftlr;
    /* SPI Receive FIFO Threshold Level                          (0x1c)*/
    volatile uint32_t rxftlr;
    /* SPI Transmit FIFO Level Register                          (0x20)*/
    volatile uint32_t txflr;
    /* SPI Receive FIFO Level Register                           (0x24)*/
    volatile uint32_t rxflr;
    /* SPI Status Register                                       (0x28)*/
    volatile uint32_t sr;
    /* SPI Interrupt Mask Register                               (0x2c)*/
    volatile uint32_t imr;
    /* SPI Interrupt Status Register                             (0x30)*/
    volatile uint32_t isr;
    /* SPI Raw Interrupt Status Register                         (0x34)*/
    volatile uint32_t risr;
    /* SPI Transmit FIFO Overflow Interrupt Clear Register       (0x38)*/
    volatile uint32_t txoicr;
    /* SPI Receive FIFO Overflow Interrupt Clear Register        (0x3c)*/
    volatile uint32_t rxoicr;
    /* SPI Receive FIFO Underflow Interrupt Clear Register       (0x40)*/
    volatile uint32_t rxuicr;
    /* SPI Multi-Master Interrupt Clear Register                 (0x44)*/
    volatile uint32_t msticr;
    /* SPI Interrupt Clear Register                              (0x48)*/
    volatile uint32_t icr;
    /* SPI DMA Control Register                                  (0x4c)*/
    volatile uint32_t dmacr;
    /* SPI DMA Transmit Data Level                               (0x50)*/
    volatile uint32_t dmatdlr;
    /* SPI DMA Receive Data Level                                (0x54)*/
    volatile uint32_t dmardlr;
    /* SPI Identification Register                               (0x58)*/
    volatile uint32_t idr;
    /* SPI DWC_ssi component version                             (0x5c)*/
    volatile uint32_t ssic_version_id;
    /* SPI Data Register 0-36                                    (0x60 -- 0xec)*/
    volatile uint32_t dr[36];
    /* SPI RX Sample Delay Register                              (0xf0)*/
    volatile uint32_t rx_sample_delay;
    /* SPI SPI Control Register                                  (0xf4)*/
    volatile uint32_t spi_ctrlr0;
    /* reserved                                                  (0xf8)*/
    volatile uint32_t resv;
    /* SPI XIP Mode bits                                         (0xfc)*/
    volatile uint32_t xip_mode_bits;
    /* SPI XIP INCR transfer opcode                              (0x100)*/
    volatile uint32_t xip_incr_inst;
    /* SPI XIP WRAP transfer opcode                              (0x104)*/
    volatile uint32_t xip_wrap_inst;
    /* SPI XIP Control Register                                  (0x108)*/
    volatile uint32_t xip_ctrl;
    /* SPI XIP Slave Enable Register                             (0x10c)*/
    volatile uint32_t xip_ser;
    /* SPI XIP Receive FIFO Overflow Interrupt Clear Register    (0x110)*/
    volatile uint32_t xrxoicr;
    /* SPI XIP time out register for continuous transfers        (0x114)*/
    volatile uint32_t xip_cnt_time_out;
    volatile uint32_t endian;
} __attribute__((packed, aligned(4))) spi_t;

typedef enum
{
    SPI_DEVICE_0,
    SPI_DEVICE_1,
    SPI_DEVICE_2,
    SPI_DEVICE_3,
    SPI_DEVICE_MAX,
} spi_device_num_t;

typedef enum
{
    SPI_WORK_MODE_0,
    SPI_WORK_MODE_1,
    SPI_WORK_MODE_2,
    SPI_WORK_MODE_3,
} spi_work_mode_t;

typedef enum
{
    SPI_FF_STANDARD,
    SPI_FF_DUAL,
    SPI_FF_QUAD,
    SPI_FF_OCTAL,
} spi_frame_format_t;

typedef enum
{
    SPI_TMODE_TRANS_RECV,
    SPI_TMODE_TRANS,
    SPI_TMODE_RECV,
    SPI_TMODE_EEROM,
} spi_transfer_mode_t;

typedef enum
{
    SPI_CHIP_SELECT_0,
    SPI_CHIP_SELECT_1,
    SPI_CHIP_SELECT_2,
    SPI_CHIP_SELECT_3,
    SPI_CHIP_SELECT_MAX,
} spi_chip_select_t;

extern volatile spi_t *const spi[4];

/**
 * @brief       Set spi configuration
 *
 * @param[in]   spi_num             Spi bus number
 * @param[in]   mode                Spi mode
 * @param[in]   frame_format        Spi frame format
 * @param[in]   data_bit_length     Spi data bit length
 * @param[in]   endian              0:little-endian 1:big-endian
 *
 * @return      Void
 */
void spi_init(spi_device_num_t spi_num, spi_work_mode_t work_mode, spi_frame_format_t frame_format,
              size_t data_bit_length, uint32_t endian);

/**
 * @brief       Spi send data
 *
 * @param[in]   spi_num         Spi bus number
 * @param[in]   slave     Spi chip select
 * @param[in]   cmd_buff        Spi command buffer point
 * @param[in]   cmd_len         Spi command length
 * @param[in]   tx_buff         Spi transmit buffer point
 * @param[in]   tx_len          Spi transmit buffer length
 *
 * @return      Result
 *     - 0      Success
 *     - Other  Fail
 */
void spi_send_data_standard(spi_device_num_t spi_num, spi_chip_select_t slave, const uint8_t *tx_buff, size_t tx_len);

/**
 * @brief       Spi receive data
 *
 * @param[in]   spi_num             Spi bus number
 * @param[in]   chip_select         Spi chip select
 * @param[in]   rx_buff             Spi receive buffer point
 * @param[in]   rx_len              Spi receive buffer length
 *
 * @return      Result
 *     - 0      Success
 *     - Other  Fail
 */
void spi_receive_data_standard(spi_device_num_t spi_num, spi_chip_select_t chip_select, uint8_t *rx_buff, size_t rx_len);

/**
 * @brief       Spi normal send by dma
 *
 * @param[in]   spi_num         Spi bus number
 * @param[in]   spi_clk         Spi clock rate
 *
 * @return      The real spi clock rate
 */
uint32_t spi_set_clk_rate(spi_device_num_t spi_num, uint32_t spi_clk);

#endif //LAB8_SPI_H
