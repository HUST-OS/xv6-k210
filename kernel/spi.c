// SPI Protocol Implementation

#include "include/types.h"
#include "include/spi.h"
#include "include/platform.h"
#include "include/riscv.h"
#include "include/utils.h"
#include "include/defs.h"

volatile spi_t *const spi[4] =
    {
        (volatile spi_t *)SPI0_BASE_ADDR,
        (volatile spi_t *)SPI1_BASE_ADDR,
        (volatile spi_t *)SPI_SLAVE_BASE_ADDR,
        (volatile spi_t *)SPI3_BASE_ADDR};

void spi_init(spi_device_num_t spi_num, spi_work_mode_t work_mode, spi_frame_format_t frame_format,
              uint64 data_bit_length, uint32 endian)
{
    // configASSERT(data_bit_length >= 4 && data_bit_length <= 32);
    // configASSERT(spi_num < SPI_DEVICE_MAX && spi_num != 2);
    // spi_clk_init(spi_num);

    // uint8 dfs_offset, frf_offset, work_mode_offset;
    uint8 dfs_offset = 0;
    uint8 frf_offset = 0;
    uint8 work_mode_offset = 0;
    switch(spi_num)
    {
        case 0:
        case 1:
            dfs_offset = 16;
            frf_offset = 21;
            work_mode_offset = 6;
            break;
        case 2:
            // configASSERT(!"Spi Bus 2 Not Support!");
            break;
        case 3:
        default:
            dfs_offset = 0;
            frf_offset = 22;
            work_mode_offset = 8;
            break;
    }

    switch(frame_format)
    {
        case SPI_FF_DUAL:
            // configASSERT(data_bit_length % 2 == 0);
            break;
        case SPI_FF_QUAD:
            // configASSERT(data_bit_length % 4 == 0);
            break;
        case SPI_FF_OCTAL:
            // configASSERT(data_bit_length % 8 == 0);
            break;
        default:
            break;
    }
    volatile spi_t *spi_adapter = spi[spi_num];
    if(spi_adapter->baudr == 0)
        spi_adapter->baudr = 0x14;
    spi_adapter->imr = 0x00;
    spi_adapter->dmacr = 0x00;
    spi_adapter->dmatdlr = 0x10;
    spi_adapter->dmardlr = 0x00;
    spi_adapter->ser = 0x00;
    spi_adapter->ssienr = 0x00;
    spi_adapter->ctrlr0 = (work_mode << work_mode_offset) | (frame_format << frf_offset) | ((data_bit_length - 1) << dfs_offset);
    spi_adapter->spi_ctrlr0 = 0;
    spi_adapter->endian = endian;
}


static void spi_set_tmod(uint8 spi_num, uint32 tmod)
{
    // configASSERT(spi_num < SPI_DEVICE_MAX);
    volatile spi_t *spi_handle = spi[spi_num];
    uint8 tmod_offset = 0;
    switch(spi_num)
    {
        case 0:
        case 1:
        case 2:
            tmod_offset = 8;
            break;
        case 3:
        default:
            tmod_offset = 10;
            break;
    }
    set_bit(&spi_handle->ctrlr0, 3 << tmod_offset, tmod << tmod_offset);
}

static spi_transfer_width_t spi_get_frame_size(uint64 data_bit_length)
{
    if(data_bit_length < 8)
        return SPI_TRANS_CHAR;
    else if(data_bit_length < 16)
        return SPI_TRANS_SHORT;
    return SPI_TRANS_INT;
}

void spi_send_data_normal(spi_device_num_t spi_num, spi_chip_select_t chip_select, const uint8 *tx_buff, uint64 tx_len)
{
    // configASSERT(spi_num < SPI_DEVICE_MAX && spi_num != 2);

    uint64 index, fifo_len;
    spi_set_tmod(spi_num, SPI_TMOD_TRANS);

    volatile spi_t *spi_handle = spi[spi_num];

    // uint8 dfs_offset;
    uint8 dfs_offset = 0;
    switch(spi_num)
    {
        case 0:
        case 1:
            dfs_offset = 16;
            break;
        case 2:
            // configASSERT(!"Spi Bus 2 Not Support!");
            break;
        case 3:
        default:
            dfs_offset = 0;
            break;
    }
    uint32 data_bit_length = (spi_handle->ctrlr0 >> dfs_offset) & 0x1F;
    spi_transfer_width_t frame_width = spi_get_frame_size(data_bit_length);

    uint8 v_misalign_flag = 0;
    uint32 v_send_data;
    if((uintptr_t)tx_buff % frame_width)
        v_misalign_flag = 1;

    spi_handle->ssienr = 0x01;
    spi_handle->ser = 1U << chip_select;
    uint32 i = 0;
    while(tx_len)
    {
        fifo_len = 32 - spi_handle->txflr;
        fifo_len = fifo_len < tx_len ? fifo_len : tx_len;
        switch(frame_width)
        {
            case SPI_TRANS_INT:
                fifo_len = fifo_len / 4 * 4;
                if(v_misalign_flag)
                {
                    for(index = 0; index < fifo_len; index += 4)
                    {
                        // memcpy(&v_send_data, tx_buff + i, 4);
                        memmove(&v_send_data, tx_buff + i, 4);
                        spi_handle->dr[0] = v_send_data;
                        i += 4;
                    }
                } else
                {
                    for(index = 0; index < fifo_len / 4; index++)
                        spi_handle->dr[0] = ((uint32 *)tx_buff)[i++];
                }
                break;
            case SPI_TRANS_SHORT:
                fifo_len = fifo_len / 2 * 2;
                if(v_misalign_flag)
                {
                    for(index = 0; index < fifo_len; index += 2)
                    {
                        // memcpy(&v_send_data, tx_buff + i, 2);
                        memmove(&v_send_data, tx_buff + i, 2);
                        spi_handle->dr[0] = v_send_data;
                        i += 2;
                    }
                } else
                {
                    for(index = 0; index < fifo_len / 2; index++)
                        spi_handle->dr[0] = ((uint16 *)tx_buff)[i++];
                }
                break;
            default:
                for(index = 0; index < fifo_len; index++)
                    spi_handle->dr[0] = tx_buff[i++];
                break;
        }
        tx_len -= fifo_len;
    }
    while((spi_handle->sr & 0x05) != 0x04)
        ;
    spi_handle->ser = 0x00;
    spi_handle->ssienr = 0x00;
}

void spi_send_data_standard(spi_device_num_t spi_num, spi_chip_select_t chip_select, const uint8 *cmd_buff,
                            uint64 cmd_len, const uint8 *tx_buff, uint64 tx_len)
{
    // configASSERT(spi_num < SPI_DEVICE_MAX && spi_num != 2);
    // uint8 *v_buf = malloc(cmd_len + tx_len);
    uint8 *v_buf = kalloc();
    uint64 i;
    for(i = 0; i < cmd_len; i++)
        v_buf[i] = cmd_buff[i];
    for(i = 0; i < tx_len; i++)
        v_buf[cmd_len + i] = tx_buff[i];

    spi_send_data_normal(spi_num, chip_select, v_buf, cmd_len + tx_len);
    // free((void *)v_buf);
    kfree((void *)v_buf);
}

void spi_receive_data_standard(spi_device_num_t spi_num, spi_chip_select_t chip_select, const uint8 *cmd_buff,
                               uint64 cmd_len, uint8 *rx_buff, uint64 rx_len)
{
    // configASSERT(spi_num < SPI_DEVICE_MAX && spi_num != 2);
    uint64 index, fifo_len;
    if(cmd_len == 0)
        spi_set_tmod(spi_num, SPI_TMOD_RECV);
    else
        spi_set_tmod(spi_num, SPI_TMOD_EEROM);
    volatile spi_t *spi_handle = spi[spi_num];

    // uint8 dfs_offset;
    uint8 dfs_offset = 0;
    switch(spi_num)
    {
        case 0:
        case 1:
            dfs_offset = 16;
            break;
        case 2:
            // configASSERT(!"Spi Bus 2 Not Support!");
            break;
        case 3:
        default:
            dfs_offset = 0;
            break;
    }
    uint32 data_bit_length = (spi_handle->ctrlr0 >> dfs_offset) & 0x1F;
    spi_transfer_width_t frame_width = spi_get_frame_size(data_bit_length);

    uint32 i = 0;
    uint64 v_cmd_len = cmd_len / frame_width;
    uint32 v_rx_len = rx_len / frame_width;

    spi_handle->ctrlr1 = (uint32)(v_rx_len - 1);
    spi_handle->ssienr = 0x01;

    while(v_cmd_len)
    {
        fifo_len = 32 - spi_handle->txflr;
        fifo_len = fifo_len < v_cmd_len ? fifo_len : v_cmd_len;
        switch(frame_width)
        {
            case SPI_TRANS_INT:
                for(index = 0; index < fifo_len; index++)
                    spi_handle->dr[0] = ((uint32 *)cmd_buff)[i++];
                break;
            case SPI_TRANS_SHORT:
                for(index = 0; index < fifo_len; index++)
                    spi_handle->dr[0] = ((uint16 *)cmd_buff)[i++];
                break;
            default:
                for(index = 0; index < fifo_len; index++)
                    spi_handle->dr[0] = cmd_buff[i++];
                break;
        }
        spi_handle->ser = 1U << chip_select;
        v_cmd_len -= fifo_len;
    }

    if(cmd_len == 0)
    {
        spi_handle->dr[0] = 0xffffffff;
        spi_handle->ser = 1U << chip_select;
    }

    i = 0;
    while(v_rx_len)
    {
        fifo_len = spi_handle->rxflr;
        fifo_len = fifo_len < v_rx_len ? fifo_len : v_rx_len;
        switch(frame_width)
        {
            case SPI_TRANS_INT:
                for(index = 0; index < fifo_len; index++)
                    ((uint32 *)rx_buff)[i++] = spi_handle->dr[0];
                break;
            case SPI_TRANS_SHORT:
                for(index = 0; index < fifo_len; index++)
                    ((uint16 *)rx_buff)[i++] = (uint16)spi_handle->dr[0];
                break;
            default:
                for(index = 0; index < fifo_len; index++)
                    rx_buff[i++] = (uint8)spi_handle->dr[0];
                break;
        }

        v_rx_len -= fifo_len;
    }

    spi_handle->ser = 0x00;
    spi_handle->ssienr = 0x00;
}