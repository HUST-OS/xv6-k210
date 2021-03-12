/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "include/types.h"
#include "include/encoding.h"
#include "include/sysctl.h"
#include "include/printf.h"

#define SYSCTRL_CLOCK_FREQ_IN0 (26000000UL)

const uint8 get_select_pll2[] =
    {
        [SYSCTL_SOURCE_IN0] = 0,
        [SYSCTL_SOURCE_PLL0] = 1,
        [SYSCTL_SOURCE_PLL1] = 2,
};

const uint8 get_source_pll2[] =
    {
        [0] = SYSCTL_SOURCE_IN0,
        [1] = SYSCTL_SOURCE_PLL0,
        [2] = SYSCTL_SOURCE_PLL1,
};

const uint8 get_select_aclk[] =
    {
        [SYSCTL_SOURCE_IN0] = 0,
        [SYSCTL_SOURCE_PLL0] = 1,
};

const uint8 get_source_aclk[] =
    {
        [0] = SYSCTL_SOURCE_IN0,
        [1] = SYSCTL_SOURCE_PLL0,
};

volatile sysctl_t *const sysctl = (volatile sysctl_t *)SYSCTL_BASE_ADDR;

uint32 sysctl_get_git_id(void)
{
    return sysctl->git_id.git_id;
}

uint32 sysctl_get_freq(void)
{
    return sysctl->clk_freq.clk_freq;
}

static int sysctl_clock_bus_en(sysctl_clock_t clock, uint8 en)
{
    /*
     * The timer is under APB0, to prevent apb0_clk_en1 and apb0_clk_en0
     * on same register, we split it to peripheral and central two
     * registers, to protect CPU close apb0 clock accidentally.
     *
     * The apb0_clk_en0 and apb0_clk_en1 have same function,
     * one of them set, the APB0 clock enable.
     */

    /* The APB clock should carefully disable */
    if(en)
    {
        switch(clock)
        {
            /*
             * These peripheral devices are under APB0
             * GPIO, UART1, UART2, UART3, SPI_SLAVE, I2S0, I2S1,
             * I2S2, I2C0, I2C1, I2C2, FPIOA, SHA256, TIMER0,
             * TIMER1, TIMER2
             */
            case SYSCTL_CLOCK_GPIO:
            case SYSCTL_CLOCK_SPI2:
            case SYSCTL_CLOCK_I2S0:
            case SYSCTL_CLOCK_I2S1:
            case SYSCTL_CLOCK_I2S2:
            case SYSCTL_CLOCK_I2C0:
            case SYSCTL_CLOCK_I2C1:
            case SYSCTL_CLOCK_I2C2:
            case SYSCTL_CLOCK_UART1:
            case SYSCTL_CLOCK_UART2:
            case SYSCTL_CLOCK_UART3:
            case SYSCTL_CLOCK_FPIOA:
            case SYSCTL_CLOCK_TIMER0:
            case SYSCTL_CLOCK_TIMER1:
            case SYSCTL_CLOCK_TIMER2:
            case SYSCTL_CLOCK_SHA:
                sysctl->clk_en_cent.apb0_clk_en = en;
                break;

            /*
             * These peripheral devices are under APB1
             * WDT, AES, OTP, DVP, SYSCTL
             */
            case SYSCTL_CLOCK_AES:
            case SYSCTL_CLOCK_WDT0:
            case SYSCTL_CLOCK_WDT1:
            case SYSCTL_CLOCK_OTP:
            case SYSCTL_CLOCK_RTC:
                sysctl->clk_en_cent.apb1_clk_en = en;
                break;

            /*
             * These peripheral devices are under APB2
             * SPI0, SPI1
             */
            case SYSCTL_CLOCK_SPI0:
            case SYSCTL_CLOCK_SPI1:
                sysctl->clk_en_cent.apb2_clk_en = en;
                break;

            default:
                break;
        }
    }

    return 0;
}

static int sysctl_clock_device_en(sysctl_clock_t clock, uint8 en)
{
    switch(clock)
    {
        /*
         * These devices are PLL
         */
        case SYSCTL_CLOCK_PLL0:
            sysctl->pll0.pll_out_en0 = en;
            break;
        case SYSCTL_CLOCK_PLL1:
            sysctl->pll1.pll_out_en1 = en;
            break;
        case SYSCTL_CLOCK_PLL2:
            sysctl->pll2.pll_out_en2 = en;
            break;

        /*
         * These devices are CPU, SRAM, APB bus, ROM, DMA, AI
         */
        case SYSCTL_CLOCK_CPU:
            sysctl->clk_en_cent.cpu_clk_en = en;
            break;
        case SYSCTL_CLOCK_SRAM0:
            sysctl->clk_en_cent.sram0_clk_en = en;
            break;
        case SYSCTL_CLOCK_SRAM1:
            sysctl->clk_en_cent.sram1_clk_en = en;
            break;
        case SYSCTL_CLOCK_APB0:
            sysctl->clk_en_cent.apb0_clk_en = en;
            break;
        case SYSCTL_CLOCK_APB1:
            sysctl->clk_en_cent.apb1_clk_en = en;
            break;
        case SYSCTL_CLOCK_APB2:
            sysctl->clk_en_cent.apb2_clk_en = en;
            break;
        case SYSCTL_CLOCK_ROM:
            sysctl->clk_en_peri.rom_clk_en = en;
            break;
        case SYSCTL_CLOCK_DMA:
            sysctl->clk_en_peri.dma_clk_en = en;
            break;
        case SYSCTL_CLOCK_AI:
            sysctl->clk_en_peri.ai_clk_en = en;
            break;
        case SYSCTL_CLOCK_DVP:
            sysctl->clk_en_peri.dvp_clk_en = en;
            break;
        case SYSCTL_CLOCK_FFT:
            sysctl->clk_en_peri.fft_clk_en = en;
            break;
        case SYSCTL_CLOCK_SPI3:
            sysctl->clk_en_peri.spi3_clk_en = en;
            break;

        /*
         * These peripheral devices are under APB0
         * GPIO, UART1, UART2, UART3, SPI_SLAVE, I2S0, I2S1,
         * I2S2, I2C0, I2C1, I2C2, FPIOA, SHA256, TIMER0,
         * TIMER1, TIMER2
         */
        case SYSCTL_CLOCK_GPIO:
            sysctl->clk_en_peri.gpio_clk_en = en;
            break;
        case SYSCTL_CLOCK_SPI2:
            sysctl->clk_en_peri.spi2_clk_en = en;
            break;
        case SYSCTL_CLOCK_I2S0:
            sysctl->clk_en_peri.i2s0_clk_en = en;
            break;
        case SYSCTL_CLOCK_I2S1:
            sysctl->clk_en_peri.i2s1_clk_en = en;
            break;
        case SYSCTL_CLOCK_I2S2:
            sysctl->clk_en_peri.i2s2_clk_en = en;
            break;
        case SYSCTL_CLOCK_I2C0:
            sysctl->clk_en_peri.i2c0_clk_en = en;
            break;
        case SYSCTL_CLOCK_I2C1:
            sysctl->clk_en_peri.i2c1_clk_en = en;
            break;
        case SYSCTL_CLOCK_I2C2:
            sysctl->clk_en_peri.i2c2_clk_en = en;
            break;
        case SYSCTL_CLOCK_UART1:
            sysctl->clk_en_peri.uart1_clk_en = en;
            break;
        case SYSCTL_CLOCK_UART2:
            sysctl->clk_en_peri.uart2_clk_en = en;
            break;
        case SYSCTL_CLOCK_UART3:
            sysctl->clk_en_peri.uart3_clk_en = en;
            break;
        case SYSCTL_CLOCK_FPIOA:
            sysctl->clk_en_peri.fpioa_clk_en = en;
            break;
        case SYSCTL_CLOCK_TIMER0:
            sysctl->clk_en_peri.timer0_clk_en = en;
            break;
        case SYSCTL_CLOCK_TIMER1:
            sysctl->clk_en_peri.timer1_clk_en = en;
            break;
        case SYSCTL_CLOCK_TIMER2:
            sysctl->clk_en_peri.timer2_clk_en = en;
            break;
        case SYSCTL_CLOCK_SHA:
            sysctl->clk_en_peri.sha_clk_en = en;
            break;

        /*
         * These peripheral devices are under APB1
         * WDT, AES, OTP, DVP, SYSCTL
         */
        case SYSCTL_CLOCK_AES:
            sysctl->clk_en_peri.aes_clk_en = en;
            break;
        case SYSCTL_CLOCK_WDT0:
            sysctl->clk_en_peri.wdt0_clk_en = en;
            break;
        case SYSCTL_CLOCK_WDT1:
            sysctl->clk_en_peri.wdt1_clk_en = en;
            break;
        case SYSCTL_CLOCK_OTP:
            sysctl->clk_en_peri.otp_clk_en = en;
            break;
        case SYSCTL_CLOCK_RTC:
            sysctl->clk_en_peri.rtc_clk_en = en;
            break;

        /*
         * These peripheral devices are under APB2
         * SPI0, SPI1
         */
        case SYSCTL_CLOCK_SPI0:
            sysctl->clk_en_peri.spi0_clk_en = en;
            break;
        case SYSCTL_CLOCK_SPI1:
            sysctl->clk_en_peri.spi1_clk_en = en;
            break;

        default:
            break;
    }

    return 0;
}

int sysctl_clock_enable(sysctl_clock_t clock)
{
    if(clock >= SYSCTL_CLOCK_MAX)
        return -1;
    sysctl_clock_bus_en(clock, 1);
    sysctl_clock_device_en(clock, 1);
    return 0;
}

int sysctl_clock_disable(sysctl_clock_t clock)
{
    if(clock >= SYSCTL_CLOCK_MAX)
        return -1;
    sysctl_clock_device_en(clock, 0);
    return 0;
}

int sysctl_clock_set_threshold(sysctl_threshold_t which, int threshold)
{
    int result = 0;
    switch(which)
    {
        /*
         * These threshold is 2 bit width
         */
        case SYSCTL_THRESHOLD_ACLK:
            sysctl->clk_sel0.aclk_divider_sel = (uint8)threshold & 0x03;
            break;

        /*
         * These threshold is 3 bit width
         */
        case SYSCTL_THRESHOLD_APB0:
            sysctl->clk_sel0.apb0_clk_sel = (uint8)threshold & 0x07;
            break;
        case SYSCTL_THRESHOLD_APB1:
            sysctl->clk_sel0.apb1_clk_sel = (uint8)threshold & 0x07;
            break;
        case SYSCTL_THRESHOLD_APB2:
            sysctl->clk_sel0.apb2_clk_sel = (uint8)threshold & 0x07;
            break;

        /*
         * These threshold is 4 bit width
         */
        case SYSCTL_THRESHOLD_SRAM0:
            sysctl->clk_th0.sram0_gclk_threshold = (uint8)threshold & 0x0F;
            break;
        case SYSCTL_THRESHOLD_SRAM1:
            sysctl->clk_th0.sram1_gclk_threshold = (uint8)threshold & 0x0F;
            break;
        case SYSCTL_THRESHOLD_AI:
            sysctl->clk_th0.ai_gclk_threshold = (uint8)threshold & 0x0F;
            break;
        case SYSCTL_THRESHOLD_DVP:
            sysctl->clk_th0.dvp_gclk_threshold = (uint8)threshold & 0x0F;
            break;
        case SYSCTL_THRESHOLD_ROM:
            sysctl->clk_th0.rom_gclk_threshold = (uint8)threshold & 0x0F;
            break;

        /*
         * These threshold is 8 bit width
         */
        case SYSCTL_THRESHOLD_SPI0:
            sysctl->clk_th1.spi0_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_SPI1:
            sysctl->clk_th1.spi1_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_SPI2:
            sysctl->clk_th1.spi2_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_SPI3:
            sysctl->clk_th1.spi3_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_TIMER0:
            sysctl->clk_th2.timer0_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_TIMER1:
            sysctl->clk_th2.timer1_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_TIMER2:
            sysctl->clk_th2.timer2_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_I2S0_M:
            sysctl->clk_th4.i2s0_mclk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_I2S1_M:
            sysctl->clk_th4.i2s1_mclk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_I2S2_M:
            sysctl->clk_th5.i2s2_mclk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_I2C0:
            sysctl->clk_th5.i2c0_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_I2C1:
            sysctl->clk_th5.i2c1_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_I2C2:
            sysctl->clk_th5.i2c2_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_WDT0:
            sysctl->clk_th6.wdt0_clk_threshold = (uint8)threshold;
            break;
        case SYSCTL_THRESHOLD_WDT1:
            sysctl->clk_th6.wdt1_clk_threshold = (uint8)threshold;
            break;

        /*
         * These threshold is 16 bit width
         */
        case SYSCTL_THRESHOLD_I2S0:
            sysctl->clk_th3.i2s0_clk_threshold = (uint16)threshold;
            break;
        case SYSCTL_THRESHOLD_I2S1:
            sysctl->clk_th3.i2s1_clk_threshold = (uint16)threshold;
            break;
        case SYSCTL_THRESHOLD_I2S2:
            sysctl->clk_th4.i2s2_clk_threshold = (uint16)threshold;
            break;

        default:
            result = -1;
            break;
    }
    return result;
}

int sysctl_clock_get_threshold(sysctl_threshold_t which)
{
    int threshold = 0;

    switch(which)
    {
        /*
         * Select and get threshold value
         */
        case SYSCTL_THRESHOLD_ACLK:
            threshold = (int)sysctl->clk_sel0.aclk_divider_sel;
            break;
        case SYSCTL_THRESHOLD_APB0:
            threshold = (int)sysctl->clk_sel0.apb0_clk_sel;
            break;
        case SYSCTL_THRESHOLD_APB1:
            threshold = (int)sysctl->clk_sel0.apb1_clk_sel;
            break;
        case SYSCTL_THRESHOLD_APB2:
            threshold = (int)sysctl->clk_sel0.apb2_clk_sel;
            break;
        case SYSCTL_THRESHOLD_SRAM0:
            threshold = (int)sysctl->clk_th0.sram0_gclk_threshold;
            break;
        case SYSCTL_THRESHOLD_SRAM1:
            threshold = (int)sysctl->clk_th0.sram1_gclk_threshold;
            break;
        case SYSCTL_THRESHOLD_AI:
            threshold = (int)sysctl->clk_th0.ai_gclk_threshold;
            break;
        case SYSCTL_THRESHOLD_DVP:
            threshold = (int)sysctl->clk_th0.dvp_gclk_threshold;
            break;
        case SYSCTL_THRESHOLD_ROM:
            threshold = (int)sysctl->clk_th0.rom_gclk_threshold;
            break;
        case SYSCTL_THRESHOLD_SPI0:
            threshold = (int)sysctl->clk_th1.spi0_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_SPI1:
            threshold = (int)sysctl->clk_th1.spi1_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_SPI2:
            threshold = (int)sysctl->clk_th1.spi2_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_SPI3:
            threshold = (int)sysctl->clk_th1.spi3_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_TIMER0:
            threshold = (int)sysctl->clk_th2.timer0_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_TIMER1:
            threshold = (int)sysctl->clk_th2.timer1_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_TIMER2:
            threshold = (int)sysctl->clk_th2.timer2_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2S0:
            threshold = (int)sysctl->clk_th3.i2s0_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2S1:
            threshold = (int)sysctl->clk_th3.i2s1_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2S2:
            threshold = (int)sysctl->clk_th4.i2s2_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2S0_M:
            threshold = (int)sysctl->clk_th4.i2s0_mclk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2S1_M:
            threshold = (int)sysctl->clk_th4.i2s1_mclk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2S2_M:
            threshold = (int)sysctl->clk_th5.i2s2_mclk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2C0:
            threshold = (int)sysctl->clk_th5.i2c0_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2C1:
            threshold = (int)sysctl->clk_th5.i2c1_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_I2C2:
            threshold = (int)sysctl->clk_th5.i2c2_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_WDT0:
            threshold = (int)sysctl->clk_th6.wdt0_clk_threshold;
            break;
        case SYSCTL_THRESHOLD_WDT1:
            threshold = (int)sysctl->clk_th6.wdt1_clk_threshold;
            break;

        default:
            break;
    }

    return threshold;
}

int sysctl_clock_set_clock_select(sysctl_clock_select_t which, int select)
{
    int result = 0;
    switch(which)
    {
        /*
         * These clock select is 1 bit width
         */
        case SYSCTL_CLOCK_SELECT_PLL0_BYPASS:
            sysctl->pll0.pll_bypass0 = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_PLL1_BYPASS:
            sysctl->pll1.pll_bypass1 = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_PLL2_BYPASS:
            sysctl->pll2.pll_bypass2 = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_ACLK:
            sysctl->clk_sel0.aclk_sel = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_SPI3:
            sysctl->clk_sel0.spi3_clk_sel = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_TIMER0:
            sysctl->clk_sel0.timer0_clk_sel = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_TIMER1:
            sysctl->clk_sel0.timer1_clk_sel = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_TIMER2:
            sysctl->clk_sel0.timer2_clk_sel = select & 0x01;
            break;
        case SYSCTL_CLOCK_SELECT_SPI3_SAMPLE:
            sysctl->clk_sel1.spi3_sample_clk_sel = select & 0x01;
            break;

        /*
         * These clock select is 2 bit width
         */
        case SYSCTL_CLOCK_SELECT_PLL2:
            sysctl->pll2.pll_ckin_sel2 = select & 0x03;
            break;

        default:
            result = -1;
            break;
    }

    return result;
}

int sysctl_clock_get_clock_select(sysctl_clock_select_t which)
{
    int clock_select = 0;

    switch(which)
    {
        /*
         * Select and get clock select value
         */
        case SYSCTL_CLOCK_SELECT_PLL0_BYPASS:
            clock_select = (int)sysctl->pll0.pll_bypass0;
            break;
        case SYSCTL_CLOCK_SELECT_PLL1_BYPASS:
            clock_select = (int)sysctl->pll1.pll_bypass1;
            break;
        case SYSCTL_CLOCK_SELECT_PLL2_BYPASS:
            clock_select = (int)sysctl->pll2.pll_bypass2;
            break;
        case SYSCTL_CLOCK_SELECT_PLL2:
            clock_select = (int)sysctl->pll2.pll_ckin_sel2;
            break;
        case SYSCTL_CLOCK_SELECT_ACLK:
            clock_select = (int)sysctl->clk_sel0.aclk_sel;
            break;
        case SYSCTL_CLOCK_SELECT_SPI3:
            clock_select = (int)sysctl->clk_sel0.spi3_clk_sel;
            break;
        case SYSCTL_CLOCK_SELECT_TIMER0:
            clock_select = (int)sysctl->clk_sel0.timer0_clk_sel;
            break;
        case SYSCTL_CLOCK_SELECT_TIMER1:
            clock_select = (int)sysctl->clk_sel0.timer1_clk_sel;
            break;
        case SYSCTL_CLOCK_SELECT_TIMER2:
            clock_select = (int)sysctl->clk_sel0.timer2_clk_sel;
            break;
        case SYSCTL_CLOCK_SELECT_SPI3_SAMPLE:
            clock_select = (int)sysctl->clk_sel1.spi3_sample_clk_sel;
            break;

        default:
            break;
    }

    return clock_select;
}

uint32 sysctl_clock_source_get_freq(sysctl_clock_source_t input)
{
    uint32 result;

    switch(input)
    {
        case SYSCTL_SOURCE_IN0:
            result = SYSCTRL_CLOCK_FREQ_IN0;
            break;
        case SYSCTL_SOURCE_PLL0:
            result = sysctl_pll_get_freq(SYSCTL_PLL0);
            break;
        case SYSCTL_SOURCE_PLL1:
            result = sysctl_pll_get_freq(SYSCTL_PLL1);
            break;
        case SYSCTL_SOURCE_PLL2:
            result = sysctl_pll_get_freq(SYSCTL_PLL2);
            break;
        case SYSCTL_SOURCE_ACLK:
            result = sysctl_clock_get_freq(SYSCTL_CLOCK_ACLK);
            break;
        default:
            result = 0;
            break;
    }
    return result;
}

static int sysctl_pll_is_lock(sysctl_pll_t pll)
{
    /*
     * All bit enable means PLL lock
     *
     * struct pll_lock_t
     * {
     *         uint8 overflow : 1;
     *         uint8 rfslip : 1;
     *         uint8 fbslip : 1;
     * };
     *
     */

    if(pll >= SYSCTL_PLL_MAX)
        return 0;

    switch(pll)
    {
        case SYSCTL_PLL0:
            return sysctl->pll_lock.pll_lock0 == 3;

        case SYSCTL_PLL1:
            return sysctl->pll_lock.pll_lock1 & 1;

        case SYSCTL_PLL2:
            return sysctl->pll_lock.pll_lock2 & 1;

        default:
            break;
    }

    return 0;
}

static int sysctl_pll_clear_slip(sysctl_pll_t pll)
{
    if(pll >= SYSCTL_PLL_MAX)
        return -1;

    switch(pll)
    {
        case SYSCTL_PLL0:
            sysctl->pll_lock.pll_slip_clear0 = 1;
            break;

        case SYSCTL_PLL1:
            sysctl->pll_lock.pll_slip_clear1 = 1;
            break;

        case SYSCTL_PLL2:
            sysctl->pll_lock.pll_slip_clear2 = 1;
            break;

        default:
            break;
    }

    return sysctl_pll_is_lock(pll) ? 0 : -1;
}

int sysctl_pll_enable(sysctl_pll_t pll)
{
    /*
     *       ---+
     * PWRDN    |
     *          +-------------------------------------------------------------
     *          ^
     *          |
     *          |
     *          t1
     *                 +------------------+
     * RESET           |                  |
     *       ----------+                  +-----------------------------------
     *                 ^                  ^                              ^
     *                 |<----- t_rst ---->|<---------- t_lock ---------->|
     *                 |                  |                              |
     *                 t2                 t3                             t4
     */

    if(pll >= SYSCTL_PLL_MAX)
        return -1;

    switch(pll)
    {
        case SYSCTL_PLL0:
            /* Do not bypass PLL */
            sysctl->pll0.pll_bypass0 = 0;
            /*
             * Power on the PLL, negtive from PWRDN
             * 0 is power off
             * 1 is power on
             */
            sysctl->pll0.pll_pwrd0 = 1;
            /*
             * Reset trigger of the PLL, connected RESET
             * 0 is free
             * 1 is reset
             */
            sysctl->pll0.pll_reset0 = 0;
            sysctl->pll0.pll_reset0 = 1;
            asm volatile("nop");
            asm volatile("nop");
            sysctl->pll0.pll_reset0 = 0;
            break;

        case SYSCTL_PLL1:
            /* Do not bypass PLL */
            sysctl->pll1.pll_bypass1 = 0;
            /*
             * Power on the PLL, negtive from PWRDN
             * 0 is power off
             * 1 is power on
             */
            sysctl->pll1.pll_pwrd1 = 1;
            /*
             * Reset trigger of the PLL, connected RESET
             * 0 is free
             * 1 is reset
             */
            sysctl->pll1.pll_reset1 = 0;
            sysctl->pll1.pll_reset1 = 1;
            asm volatile("nop");
            asm volatile("nop");
            sysctl->pll1.pll_reset1 = 0;
            break;

        case SYSCTL_PLL2:
            /* Do not bypass PLL */
            sysctl->pll2.pll_bypass2 = 0;
            /*
             * Power on the PLL, negtive from PWRDN
             * 0 is power off
             * 1 is power on
             */
            sysctl->pll2.pll_pwrd2 = 1;
            /*
             * Reset trigger of the PLL, connected RESET
             * 0 is free
             * 1 is reset
             */
            sysctl->pll2.pll_reset2 = 0;
            sysctl->pll2.pll_reset2 = 1;
            asm volatile("nop");
            asm volatile("nop");
            sysctl->pll2.pll_reset2 = 0;
            break;

        default:
            break;
    }

    return 0;
}

int sysctl_pll_disable(sysctl_pll_t pll)
{
    if(pll >= SYSCTL_PLL_MAX)
        return -1;

    switch(pll)
    {
        case SYSCTL_PLL0:
            /* Bypass PLL */
            sysctl->pll0.pll_bypass0 = 1;
            /*
             * Power on the PLL, negtive from PWRDN
             * 0 is power off
             * 1 is power on
             */
            sysctl->pll0.pll_pwrd0 = 0;
            break;

        case SYSCTL_PLL1:
            /* Bypass PLL */
            sysctl->pll1.pll_bypass1 = 1;
            /*
             * Power on the PLL, negtive from PWRDN
             * 0 is power off
             * 1 is power on
             */
            sysctl->pll1.pll_pwrd1 = 0;
            break;

        case SYSCTL_PLL2:
            /* Bypass PLL */
            sysctl->pll2.pll_bypass2 = 1;
            /*
             * Power on the PLL, negtive from PWRDN
             * 0 is power off
             * 1 is power on
             */
            sysctl->pll2.pll_pwrd2 = 0;
            break;

        default:
            break;
    }

    return 0;
}

uint32 sysctl_pll_get_freq(sysctl_pll_t pll)
{
    uint32 freq_in = 0, freq_out = 0;
    uint32 nr = 0, nf = 0, od = 0;
    uint8 select = 0;

    if(pll >= SYSCTL_PLL_MAX)
        return 0;

    switch(pll)
    {
        case SYSCTL_PLL0:
            freq_in = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
            nr = sysctl->pll0.clkr0 + 1;
            nf = sysctl->pll0.clkf0 + 1;
            od = sysctl->pll0.clkod0 + 1;
            break;

        case SYSCTL_PLL1:
            freq_in = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
            nr = sysctl->pll1.clkr1 + 1;
            nf = sysctl->pll1.clkf1 + 1;
            od = sysctl->pll1.clkod1 + 1;
            break;

        case SYSCTL_PLL2:
            /*
             * Get input freq accroding select register
             */
            select = sysctl->pll2.pll_ckin_sel2;
            if(select < sizeof(get_source_pll2))
                freq_in = sysctl_clock_source_get_freq(get_source_pll2[select]);
            else
                return 0;

            nr = sysctl->pll2.clkr2 + 1;
            nf = sysctl->pll2.clkf2 + 1;
            od = sysctl->pll2.clkod2 + 1;
            break;

        default:
            break;
    }

    /*
     * Get final PLL output freq
     * FOUT = FIN / NR * NF / OD
     */
    freq_out = (double)freq_in / (double)nr * (double)nf / (double)od;
    return freq_out;
}

uint32 sysctl_clock_get_freq(sysctl_clock_t clock)
{
    uint32 source = 0;
    uint32 result = 0;

    switch(clock)
    {
        /*
         * The clock IN0
         */
        case SYSCTL_CLOCK_IN0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
            result = source;
            break;

        /*
         * These clock directly under PLL clock domain
         * They are using gated divider.
         */
        case SYSCTL_CLOCK_PLL0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
            result = source;
            break;
        case SYSCTL_CLOCK_PLL1:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL1);
            result = source;
            break;
        case SYSCTL_CLOCK_PLL2:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL2);
            result = source;
            break;

        /*
         * These clock directly under ACLK clock domain
         */
        case SYSCTL_CLOCK_CPU:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_ACLK))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0) /
                             (2ULL << sysctl_clock_get_threshold(SYSCTL_THRESHOLD_ACLK));
                    break;
                default:
                    break;
            }
            result = source;
            break;
        case SYSCTL_CLOCK_DMA:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_ACLK))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0) /
                             (2ULL << sysctl_clock_get_threshold(SYSCTL_THRESHOLD_ACLK));
                    break;
                default:
                    break;
            }
            result = source;
            break;
        case SYSCTL_CLOCK_FFT:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_ACLK))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0) /
                             (2ULL << sysctl_clock_get_threshold(SYSCTL_THRESHOLD_ACLK));
                    break;
                default:
                    break;
            }
            result = source;
            break;
        case SYSCTL_CLOCK_ACLK:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_ACLK))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0) /
                             (2ULL << sysctl_clock_get_threshold(SYSCTL_THRESHOLD_ACLK));
                    break;
                default:
                    break;
            }
            result = source;
            break;
        case SYSCTL_CLOCK_HCLK:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_ACLK))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0) /
                             (2ULL << sysctl_clock_get_threshold(SYSCTL_THRESHOLD_ACLK));
                    break;
                default:
                    break;
            }
            result = source;
            break;

        /*
         * These clock under ACLK clock domain.
         * They are using gated divider.
         */
        case SYSCTL_CLOCK_SRAM0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_ACLK);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_SRAM0) + 1);
            break;
        case SYSCTL_CLOCK_SRAM1:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_ACLK);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_SRAM1) + 1);
            break;
        case SYSCTL_CLOCK_ROM:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_ACLK);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_ROM) + 1);
            break;
        case SYSCTL_CLOCK_DVP:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_ACLK);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_DVP) + 1);
            break;

        /*
         * These clock under ACLK clock domain.
         * They are using even divider.
         */
        case SYSCTL_CLOCK_APB0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_ACLK);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_APB0) + 1);
            break;
        case SYSCTL_CLOCK_APB1:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_ACLK);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_APB1) + 1);
            break;
        case SYSCTL_CLOCK_APB2:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_ACLK);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_APB2) + 1);
            break;

        /*
         * These clock under AI clock domain.
         * They are using gated divider.
         */
        case SYSCTL_CLOCK_AI:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL1);
            result = source / (sysctl_clock_get_threshold(SYSCTL_THRESHOLD_AI) + 1);
            break;

        /*
         * These clock under I2S clock domain.
         * They are using even divider.
         */
        case SYSCTL_CLOCK_I2S0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL2);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2S0) + 1) * 2);
            break;
        case SYSCTL_CLOCK_I2S1:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL2);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2S1) + 1) * 2);
            break;
        case SYSCTL_CLOCK_I2S2:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL2);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2S2) + 1) * 2);
            break;

        /*
         * These clock under WDT clock domain.
         * They are using even divider.
         */
        case SYSCTL_CLOCK_WDT0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_WDT0) + 1) * 2);
            break;
        case SYSCTL_CLOCK_WDT1:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_WDT1) + 1) * 2);
            break;

        /*
         * These clock under PLL0 clock domain.
         * They are using even divider.
         */
        case SYSCTL_CLOCK_SPI0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_SPI0) + 1) * 2);
            break;
        case SYSCTL_CLOCK_SPI1:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_SPI1) + 1) * 2);
            break;
        case SYSCTL_CLOCK_SPI2:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_SPI2) + 1) * 2);
            break;
        case SYSCTL_CLOCK_I2C0:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2C0) + 1) * 2);
            break;
        case SYSCTL_CLOCK_I2C1:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2C1) + 1) * 2);
            break;
        case SYSCTL_CLOCK_I2C2:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2C2) + 1) * 2);
            break;

        /*
         * These clock under PLL0_SEL clock domain.
         * They are using even divider.
         */
        case SYSCTL_CLOCK_SPI3:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_SPI3))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
                    break;
                default:
                    break;
            }

            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_SPI3) + 1) * 2);
            break;
        case SYSCTL_CLOCK_TIMER0:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_TIMER0))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
                    break;
                default:
                    break;
            }

            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_TIMER0) + 1) * 2);
            break;
        case SYSCTL_CLOCK_TIMER1:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_TIMER1))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
                    break;
                default:
                    break;
            }

            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_TIMER1) + 1) * 2);
            break;
        case SYSCTL_CLOCK_TIMER2:
            switch(sysctl_clock_get_clock_select(SYSCTL_CLOCK_SELECT_TIMER2))
            {
                case 0:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
                    break;
                case 1:
                    source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_PLL0);
                    break;
                default:
                    break;
            }

            result = source / ((sysctl_clock_get_threshold(SYSCTL_THRESHOLD_TIMER2) + 1) * 2);
            break;

        /*
         * These clock under MISC clock domain.
         * They are using even divider.
         */

        /*
         * These clock under APB0 clock domain.
         * They are using even divider.
         */
        case SYSCTL_CLOCK_GPIO:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB0);
            result = source;
            break;
        case SYSCTL_CLOCK_UART1:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB0);
            result = source;
            break;
        case SYSCTL_CLOCK_UART2:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB0);
            result = source;
            break;
        case SYSCTL_CLOCK_UART3:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB0);
            result = source;
            break;
        case SYSCTL_CLOCK_FPIOA:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB0);
            result = source;
            break;
        case SYSCTL_CLOCK_SHA:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB0);
            result = source;
            break;

        /*
         * These clock under APB1 clock domain.
         * They are using even divider.
         */
        case SYSCTL_CLOCK_AES:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB1);
            result = source;
            break;
        case SYSCTL_CLOCK_OTP:
            source = sysctl_clock_get_freq(SYSCTL_CLOCK_APB1);
            result = source;
            break;
        case SYSCTL_CLOCK_RTC:
            source = sysctl_clock_source_get_freq(SYSCTL_SOURCE_IN0);
            result = source;
            break;

        /*
         * These clock under APB2 clock domain.
         * They are using even divider.
         */
        /*
         * Do nothing.
         */
        default:
            break;
    }
    return result;
}

int sysctl_dma_select(sysctl_dma_channel_t channel, sysctl_dma_select_t select)
{
    sysctl_dma_sel0_t dma_sel0;
    sysctl_dma_sel1_t dma_sel1;

    /* Read register from bus */
    dma_sel0 = sysctl->dma_sel0;
    dma_sel1 = sysctl->dma_sel1;
    switch(channel)
    {
        case SYSCTL_DMA_CHANNEL_0:
            dma_sel0.dma_sel0 = select;
            break;

        case SYSCTL_DMA_CHANNEL_1:
            dma_sel0.dma_sel1 = select;
            break;

        case SYSCTL_DMA_CHANNEL_2:
            dma_sel0.dma_sel2 = select;
            break;

        case SYSCTL_DMA_CHANNEL_3:
            dma_sel0.dma_sel3 = select;
            break;

        case SYSCTL_DMA_CHANNEL_4:
            dma_sel0.dma_sel4 = select;
            break;

        case SYSCTL_DMA_CHANNEL_5:
            dma_sel1.dma_sel5 = select;
            break;

        default:
            return -1;
    }

    /* Write register back to bus */
    sysctl->dma_sel0 = dma_sel0;
    sysctl->dma_sel1 = dma_sel1;

    return 0;
}

uint32 sysctl_pll_fast_enable_pll(void)
{
    /*
     * Begin write PLL registers' value,
     * Using atomic write method.
     */
    sysctl_pll0_t pll0;
    sysctl_pll1_t pll1;
    sysctl_pll2_t pll2;

    /* Read register from bus */
    pll0 = sysctl->pll0;
    pll1 = sysctl->pll1;
    pll2 = sysctl->pll2;

    /* PLL VCO MAX freq: 1.8GHz */

    /* PLL0: 26M reference clk get 793M output clock */
    pll0.clkr0 = 0;
    pll0.clkf0 = 60;
    pll0.clkod0 = 1;
    pll0.bwadj0 = 60;

    /* PLL1: 26M reference clk get 390M output clock */
    pll1.clkr1 = 0;
    pll1.clkf1 = 59;
    pll1.clkod1 = 3;
    pll1.bwadj1 = 59;

    /* PLL2: 26M reference clk get 390M output clock */
    pll2.clkr2 = 0;
    pll2.clkf2 = 59;
    pll2.clkod2 = 3;
    pll2.bwadj2 = 59;

    /* Write register to bus */
    sysctl->pll0 = pll0;
    sysctl->pll1 = pll1;
    sysctl->pll2 = pll2;

    sysctl_pll_enable(SYSCTL_PLL0);
    sysctl_pll_enable(SYSCTL_PLL1);
    sysctl_pll_enable(SYSCTL_PLL2);

    while(sysctl_pll_is_lock(SYSCTL_PLL0) == 0)
        sysctl_pll_clear_slip(SYSCTL_PLL0);
    while(sysctl_pll_is_lock(SYSCTL_PLL1) == 0)
        sysctl_pll_clear_slip(SYSCTL_PLL1);
    while(sysctl_pll_is_lock(SYSCTL_PLL2) == 0)
        sysctl_pll_clear_slip(SYSCTL_PLL2);

    sysctl_clock_enable(SYSCTL_CLOCK_PLL0);
    sysctl_clock_enable(SYSCTL_CLOCK_PLL1);
    sysctl_clock_enable(SYSCTL_CLOCK_PLL2);

    /* Set ACLK to PLL0 */
    sysctl_clock_set_clock_select(SYSCTL_CLOCK_SELECT_ACLK, SYSCTL_SOURCE_PLL0);

    return 0;
}

uint32 sysctl_set_spi0_dvp_data(uint8 en)
{
    sysctl->misc.spi_dvp_data_enable = en;
    return 0;
}

void sysctl_set_power_mode(sysctl_power_bank_t power_bank, sysctl_io_power_mode_t io_power_mode)
{
    if(io_power_mode)
        *((uint32 *)(&sysctl->power_sel)) |= (1 << power_bank);
    else
        *((uint32 *)(&sysctl->power_sel)) &= ~(1 << power_bank);
}

static uint32 cpu_freq = 390000000;

uint32 sysctl_cpu_get_freq(void)
{
    return cpu_freq;
}

uint64 sysctl_get_time_us(void)
{
    uint64 v_cycle = read_cycle();
    return v_cycle * 1000000 / sysctl_clock_get_freq(SYSCTL_CLOCK_CPU);
}

sysctl_reset_enum_status_t sysctl_get_reset_status(void)
{
    static sysctl_reset_enum_status_t s_reset_status = 0;
    if(s_reset_status != 0)
    {
        return s_reset_status;
    }

    if(sysctl->reset_status.wdt0_reset_sts)
    {
        s_reset_status = SYSCTL_RESET_STATUS_WDT0;
    } else if(sysctl->reset_status.wdt1_reset_sts)
    {
        s_reset_status = SYSCTL_RESET_STATUS_WDT1;
    } else if(sysctl->reset_status.soft_reset_sts)
    {
        s_reset_status = SYSCTL_RESET_STATUS_SOFT;
    } else
    {
        s_reset_status = SYSCTL_RESET_STATUS_HARD;
    }
    sysctl->reset_status.reset_sts_clr = 1;

    return s_reset_status;
}
