// Copyright 2020 Kenta IDA
/*
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
 */
/**
 * @file lcd_backlight.hpp
*/

#ifndef LCD_BACKLIGHT_HPP__
#define LCD_BACKLIGHT_HPP__

#include <cstdint>

/**
 * @brief Controls Wio Terminal LCD back light brightness
 */
class LCDBackLight
{
private:
    std::uint8_t currentBrightness = 100;
    std::uint8_t maxBrightness = 100;
public:
    /**
     * @brief Gets current brightness
     * @return current brightness.
     */
    std::uint8_t getBrightness() const { return this->currentBrightness; }
    /**
     * @brief Gets maximum brightness
     * @return maximum brightness.
     */
    std::uint8_t getMaxBrightness() const { return this->maxBrightness; }

    /**
     * @brief Sets brightness.
     * @param [in] brightness new value of brightness. If the brightness value exceeds maximum brightness, the value will be clipped.
     *
     */
    void setBrightness(std::uint8_t brightness)
    {
        this->currentBrightness = brightness < this->maxBrightness ? brightness : this->maxBrightness;
        TC0->COUNT8.CC[0].reg = this->currentBrightness;
        while(TC0->COUNT8.SYNCBUSY.bit.CC0);
    }
    /**
     * @brief Sets maximum brightness.
     * @param [in] maxBrightness new value of maximum brightness. If the current brightness value exceeds maximum brightness, the current brightness will be clipped.
     *
     */
    void setMaxBrightness(std::uint8_t maxBrightness)
    {
        this->maxBrightness = maxBrightness;
        if( this->currentBrightness > this->maxBrightness ) {
            this->currentBrightness = this->maxBrightness;
        }
        TC0->COUNT8.PER.reg = this->maxBrightness;
        while(TC0->COUNT8.SYNCBUSY.bit.PER);
        TC0->COUNT8.CC[0].reg = this->currentBrightness;
        while(TC0->COUNT8.SYNCBUSY.bit.CC0);
    }

    /**
     * @brief Initialize hardware/peripherals to control back light brightness.
     * @remark This function must be called before using other LCDBackLight class member functions.
     */
    void initialize()
    {
        /* Enable Peripheral Clocks */
        GCLK->PCHCTRL[9].reg = 0 | (1u<<6);         // TC0, TC1
        while(!GCLK->PCHCTRL[9].bit.CHEN);
        GCLK->PCHCTRL[11].reg = 0 | (1u<<6);    // EVSYS[0]
        while(!GCLK->PCHCTRL[11].bit.CHEN);
        GCLK->PCHCTRL[33].reg = 0 | (1u<<6);    // CCL
        while(!GCLK->PCHCTRL[33].bit.CHEN);
        /* Enable Peropheral APB Clocks */
        MCLK->APBAMASK.bit.TC0_ = 1;
        MCLK->APBBMASK.bit.EVSYS_ = 1;
        MCLK->APBCMASK.bit.CCL_ = 1;

        /* Configure PORT */
        PORT->Group[2].DIRSET.reg = (1<<5);
        PORT->Group[2].EVCTRL.reg = 0x85; // PC05, OUT
        /* Configure EVSYS */
        EVSYS->USER[1].reg = 0x01;  // Channel0 -> PORT_EV0
        EVSYS->Channel[0].CHANNEL.reg = 0x74 | (0x02<<8) | (0x00<<10);  // CCL_LUTOUT0, ASYNCHRONOUS, NO_EVT_OUTPUT
        /* Configure CCL */
        CCL->CTRL.reg = (1<<0); // SWRST
        CCL->SEQCTRL[0].reg = 0x0; // Disable SEQCTRL
        CCL->LUTCTRL[0].reg = (0xaau << 24) | (1u<<22) | (0x6<<8) | (1<<1); // TRUTH=0xaa, LUTEO, INSEL0=0x06(TC), ENABLE
        CCL->CTRL.reg = (1<<1); // ENABLE

        /* Configure TC0 */
        TC0->COUNT8.CTRLA.reg = (1u<<0);   // SWRST;
        while( TC0->COUNT8.SYNCBUSY.bit.SWRST );

        TC0->COUNT8.CTRLA.reg = (0x01 << 2) | (0x01 << 4) | (0x04 << 8);   // MODE=COUNT8, PRESCALER=DIV16, PRESCSYNC=PRESC
        TC0->COUNT8.WAVE.reg  = 0x02; // WAVEGEN=NPWM;
        TC0->COUNT8.CTRLBSET.reg = (1u<<1); // LUPD
        TC0->COUNT8.PER.reg = this->maxBrightness;
        TC0->COUNT8.CC[0].reg = this->currentBrightness;
        TC0->COUNT8.CC[1].reg = 0u;
        TC0->COUNT8.DBGCTRL.bit.DBGRUN = 1;
        TC0->COUNT8.INTFLAG.reg = 0x33;    // Clear all flags
        while( TC0->COUNT8.SYNCBUSY.reg );

        TC0->COUNT8.CTRLA.bit.ENABLE = 1;   // ENABLE
        while( TC0->COUNT8.SYNCBUSY.bit.ENABLE );
    }
};
#endif //LCD_BACKLIGHT_HPP__