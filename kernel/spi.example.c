//
// Created by lumin on 2020/11/2.
//
#include <spi.h>
#include <sysctl.h>
#include <assert.h>
#include <util.h>

volatile spi_t *const spi[4] =
        {
            (volatile spi_t *)SPI0_BASE_ADDR,
            (volatile spi_t *)SPI1_BASE_ADDR,
            (volatile spi_t *)SPI_SLAVE_BASE_ADDR,
            (volatile spi_t *)SPI3_BASE_ADDR
        };

static void spi_clk_init(uint8_t spi_num);
static uint32_t spi_get_ctrlr0(spi_device_num_t spi_num, spi_work_mode_t work_mode, spi_frame_format_t frame_format, size_t data_bit_length);
static void spi_set_transfer_mode(uint8_t spi_num, spi_transfer_mode_t tmode);

void spi_init(spi_device_num_t spi_num, spi_work_mode_t work_mode, spi_frame_format_t frame_format,
              size_t data_bit_length, uint32_t endian)
{
    assert(data_bit_length >= SPI_MIN_DATA_BIT_LENGTH && data_bit_length <= SPI_MAX_DATA_BIT_LENGTH);
    assert(spi_num < SPI_DEVICE_MAX && spi_num != 2);

    spi_clk_init(spi_num);

    // init spi controller
    volatile spi_t *spi_controller = spi[spi_num];
    if (spi_controller->baudr == 0)
        spi_controller->baudr = SPI_BAUDRATE_DEFAULT_VAL;
    spi_controller->imr = SPI_INTERRUPT_DISABLE; // default disable interrupt mode
    spi_controller->dmacr = SPI_DMACR_DEFAULT_VAL;
    spi_controller->dmatdlr = SPI_DMATDLR_DEFAULT_VAL;
    spi_controller->dmardlr = SPI_DMARDLR_DEFAULT_VAL;
    spi_controller->ser = SPI_SLAVE_DISABLE; // default disable slave
    spi_controller->ssienr = SPI_MASTER_DISABLE; // default disable master
    spi_controller->ctrlr0 = spi_get_ctrlr0(spi_num, work_mode, frame_format, data_bit_length);
    spi_controller->spi_ctrlr0 = SPI_TMOD_DEFAULT_VAL; // default transmit mode
    spi_controller->endian = endian; // MSB or LSB
}

void spi_send_data_standard(spi_device_num_t spi_num, spi_chip_select_t slave, const uint8_t *tx_buff, size_t tx_len)
{
    assert(spi_num < SPI_DEVICE_MAX && spi_num != 2);
    assert(tx_len != 0);

    // set transfer mode
    spi_set_transfer_mode(spi_num,SPI_TMODE_TRANS);

    // set register status, begin to transfer
    volatile spi_t *spi_controller = spi[spi_num];
    spi_controller->ser = 1U << slave;
    spi_controller->ssienr = SPI_MASTER_ENABLE;

    // data transmission
    size_t fifo_len, i;
    uint32_t cur = 0;
    while (tx_len)
    {
        fifo_len = SPI_FIFO_CAPCITY_IN_BYTE - spi_controller->txflr;
        if (tx_len < fifo_len)
            fifo_len = tx_len;

        for (i = 0; i < fifo_len; i++)
            spi_controller->dr[0] = tx_buff[cur++];
        tx_len -= fifo_len;
    }
    // finish sign
    while ((spi_controller->sr & 0x05) != 0x04)
        ;

    spi_controller->ser = SPI_SLAVE_DISABLE;
    spi_controller->ser = SPI_MASTER_DISABLE;
}

void spi_receive_data_standard(spi_device_num_t spi_num, spi_chip_select_t chip_select, uint8_t *rx_buff, size_t rx_len)
{
    assert(spi_num < SPI_DEVICE_MAX && spi_num != 2);
    assert(rx_len != 0);

    // set transfer mode
    spi_set_transfer_mode(spi_num, SPI_TMODE_RECV);

    // set register status, begin to transfer
    volatile spi_t *spi_controller = spi[spi_num];
    spi_controller->ctrlr1 = (uint32_t)(rx_len - 1);
    spi_controller->ssienr = SPI_MASTER_ENABLE;
    spi_controller->dr[0] = 0xffffffff;
    spi_controller->ser = 1U << chip_select;

    // data transmission
    size_t fifo_len;
    uint32_t cur = 0, i;
    while (rx_len)
    {
        fifo_len = spi_controller->rxflr;
        if (rx_len < fifo_len)
            fifo_len = rx_len;

        for (i = 0; i < fifo_len; i++)
            rx_buff[cur++] = spi_controller->dr[0];
        rx_len -= fifo_len;
    }

    spi_controller->ser = SPI_SLAVE_DISABLE;
    spi_controller->ssienr = SPI_MASTER_DISABLE;
}

uint32_t spi_set_clk_rate(spi_device_num_t spi_num, uint32_t spi_clk)
{
    uint32_t spi_baudr = sysctl_clock_get_freq(SYSCTL_CLOCK_SPI0 + spi_num) / spi_clk;
    if (spi_baudr < 2)
        spi_baudr = 2;
    else if (spi_baudr > 65534)
        spi_baudr = 65534;
    volatile spi_t *spi_controller = spi[spi_num];
    spi_controller->baudr = spi_baudr;
    return sysctl_clock_get_freq(SYSCTL_CLOCK_SPI0 + spi_num) / spi_clk;
}

static void spi_clk_init(uint8_t spi_num)
{
    assert(spi_num < SPI_DEVICE_MAX && spi_num != 2);
    if (spi_num == 3)
        sysctl_clock_set_clock_select(SYSCTL_CLOCK_SELECT_SPI3, 1);
    sysctl_clock_enable(SYSCTL_CLOCK_SPI0 + spi_num);
    sysctl_clock_set_threshold(SYSCTL_CLOCK_SPI0 + spi_num, 0);
}

/*
 * calculate ctrlr0 from passed arguments
 */
static uint32_t spi_get_ctrlr0(spi_device_num_t spi_num, spi_work_mode_t work_mode, spi_frame_format_t frame_format, size_t data_bit_length)
{
    uint8_t work_mode_offset, data_bit_length_offset, frame_format_offset;
    switch (spi_num)
    {
        case 0:
        case 1:
            work_mode_offset = SPI01_WORK_MODE_OFFSET;
            data_bit_length_offset = SPI01_DATA_BIT_LENGTH_OFFSET;
            frame_format_offset = SPI01_FRAME_FORMAT_OFFSET;
            break;
        case 3:
            work_mode_offset = SPI3_WORK_MODE_OFFSET;
            data_bit_length_offset = SPI3_DATA_BIT_LENGTH_OFFSET;
            frame_format_offset = SPI3_FRAME_FORMAT_OFFSET;
            break;
        default:
            break;
    }

    switch (frame_format)
    {
        case SPI_FF_DUAL:
            assert(data_bit_length % 2 == 0);
            break;
        case SPI_FF_QUAD:
            assert(data_bit_length % 4 == 0);
            break;
        case SPI_FF_OCTAL:
            assert(data_bit_length % 8 == 0);
            break;
        default:
            break;
    }

    return (frame_format << frame_format_offset)
           | ((data_bit_length - 1) << data_bit_length_offset)
           | (work_mode << work_mode_offset);
}

static void spi_set_transfer_mode(uint8_t spi_num, spi_transfer_mode_t tmode)
{
    assert(spi_num < SPI_DEVICE_MAX);

    uint8_t tmode_offset;
    switch (spi_num)
    {
        case 0:
        case 1:
        case 2:
            tmode_offset = SPI012_TRANSFER_MODE_OFFSET;
            break;
        case 3:
            tmode_offset = SPI3_TRANSFER_MODE_OFFSET;
            break;
        default:
            break;
    }

    set_bits_value_offset(&spi[spi_num]->ctrlr0, 3, tmode, tmode_offset);
}