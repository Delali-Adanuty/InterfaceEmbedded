#include <Arduino.h>
#include "imxrt.h"
#include <stdint.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);


  CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON); // Turn on SAI1's clock; 
  // CCM_CCGR_ON is 3, which is binary 11, which sets both bits to on; 

  Serial.println(I2S1_TCSR, HEX);
  Serial.println("PLL Audio");
  Serial.println(CCM_ANALOG_PLL_AUDIO, HEX);
  Serial.println("Misc2");
  Serial.println(CCM_ANALOG_MISC2, HEX);


  CCM_ANALOG_PLL_AUDIO = (CCM_ANALOG_PLL_AUDIO & ~(
    0x0018C03F // Mask for bypass, enable, bypass clock src, post div select and div select bits (BITWISE OR)
    
  )
  |
  (
    CCM_ANALOG_PLL_AUDIO_BYPASS |
    CCM_ANALOG_PLL_AUDIO_ENABLE | 
    CCM_ANALOG_PLL_AUDIO_BYPASS_CLK_SRC(0) | 
    CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(2) | 
    CCM_ANALOG_PLL_AUDIO_DIV_SELECT(32)    
  ) // With the mask, we can set the exact bits with the OR and leave the other bits untouched
  );

  // Fout = Fx(24MHz) * (DIV_SELECT + NUM/DENOM)

  CCM_ANALOG_PLL_AUDIO_NUM = 0x0060; // This is 96 Decimal set to hex
  CCM_ANALOG_PLL_AUDIO_DENOM = 0x007D; // Ths is 125 Decimal set to hex

  CCM_ANALOG_PLL_AUDIO_CLR = CCM_ANALOG_PLL_AUDIO_POWERDOWN; //Set powerdown bit to 0

  CCM_ANALOG_MISC2 = (CCM_ANALOG_MISC2 & ~(
    0x00808000
  ) 
  ); // Sets post divider to /1


  //Track time for lock, print timeout if more than 50 milliseconds
  //endless loop until pll audio locks

  uint32_t start_time = millis();
  
  while (! (CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK) & ((millis() - start_time) < 50) ){

  };

  if (CCM_ANALOG_PLL_AUDIO_LOCK & CCM_ANALOG_PLL_AUDIO){
    Serial.println(CCM_ANALOG_PLL_AUDIO_LOCK, HEX);
    Serial.println("Elapsed time");
    Serial.println(millis() - start_time);
    Serial.println(!(CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK));


    CCM_ANALOG_PLL_AUDIO_CLR = CCM_ANALOG_PLL_AUDIO_BYPASS; // Clear analog pll audio bypass


    // Read, modify and write; 
    // Read with mask
    // Modify with the new set bits OR'd with the read value
    // Write with the assignment operator

    // Set clock to select pll audio
    CCM_CSCMR1 = (CCM_CSCMR1 & ~(0x00000C00) | CCM_CSCMR1_SAI1_CLK_SEL(2));


    //Set value for pre divider (8) and post divider(8)
    CCM_CS1CDR = (CCM_CS1CDR & ~(
      0x000001FF
    )
    |
    (
      CCM_CS1CDR_SAI1_CLK_PRED(7) |
      CCM_CS1CDR_SAI1_CLK_PODF(7)
    )
    );

    // Setting General Purpose Register 1 to SAI1 Master Clock
    IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK) | 
    (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR)); //

    /*
      We don't have to set iomux_gpr_gpr1_mclk_sel 
      because the mask clears that and 000 sets the bit to ss1_clk_root
    */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09 = 3; // Pad mux register for pin 23; MCLK 
    
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01 = 3; // Pin 7; DIN for DAC

    IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00 = 3; // Pin 8; DOUT for ADC

    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_10 = 3; // Pin 20; LRCLK

    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_11 = 3; // Pin 21; BCLK

    Serial.println(IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_00, HEX); // Select input register
  }
  else{
    Serial.println("timeout");
  }

  Serial.println(I2S1_RCSR, HEX); //I2S Receiver Register
  Serial.println(I2S1_TCSR, HEX); //I2S Transmitter Register

  //Both receiver and transmitter have to be disabled while writing other bits to the register

  I2S1_RCSR	= (I2S1_RCSR & ~(I2S_RCSR_RE)); // Disable receiver
  I2S1_TCSR = (I2S1_TCSR & ~(I2S_TCSR_TE)); // Disable transmitter

  


  uint32_t i2s_start_time = millis();

  // Give a 50ms timemout wait for the transmitter and receiver to be disabled
  while ((((I2S1_RCSR & (I2S_RCSR_RE)) || (I2S1_TCSR & (I2S_TCSR_TE)))) && (millis() - i2s_start_time) < 50){
    // Do nothing
  }

  // Check to see that both are disabled
  if (!((I2S1_RCSR & (I2S_RCSR_RE)) || (I2S1_TCSR & (I2S_TCSR_TE)))){

    // Software Reset on receiver and transmitter
    I2S1_RCSR |= I2S_RCSR_SR;
    I2S1_TCSR |= I2S_TCSR_SR; 

    Serial.println(I2S1_RCSR, HEX);
    Serial.println(I2S1_TCSR, HEX);

    // Clear receiver and transmitter; Clean slate
    I2S1_RCSR = (I2S1_RCSR & ~(I2S_RCSR_SR));
    I2S1_TCSR = (I2S1_TCSR & ~(I2S_TCSR_SR));

    I2S1_RCR2 = I2S_RCR2_SYNC(0) | I2S_RCR2_MSEL(1) | I2S_RCR2_BCP | I2S_RCR2_BCD | I2S_RCR2_DIV(1);
    Serial.println(I2S1_RCR2, HEX);
    I2S1_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF | I2S_RCR4_FSE | I2S_RCR4_FSD | I2S_RCR4_FSP;
    Serial.println(I2S1_RCR4, HEX);
    I2S1_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);	
    Serial.println(I2S1_RCR5, HEX);

    I2S1_TCR2 = I2S_TCR2_SYNC(1) | I2S_TCR2_MSEL(1) | I2S_TCR2_BCP | I2S_TCR2_BCD | I2S_TCR2_DIV(1);
    Serial.println(I2S1_TCR2, HEX);
    I2S1_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF | I2S_TCR4_FSE | I2S_TCR4_FSD | I2S_TCR4_FSP;
    Serial.println(I2S1_TCR4, HEX);
    I2S1_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);	
    Serial.println(I2S1_TCR5, HEX);    

    I2S1_TCR3 = I2S_TCR3_TCE;
    I2S1_RCR3 = I2S_RCR3_RCE;

    Serial.println(I2S1_TCR3, HEX);
    Serial.println(I2S1_RCR3, HEX);

    I2S1_RCSR |= I2S_RCSR_FR; // FIFO Reset for receiver
    I2S1_TCSR |= I2S_TCSR_FR; // FIFO Reset for transmitter

    IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2;

    IOMUXC_SAI1_RX_BCLK_SELECT_INPUT = 1;

    IOMUXC_SAI1_RX_SYNC_SELECT_INPUT = 1;

    I2S1_TCSR	|= I2S_TCSR_TE; // Enable transmitter
    I2S1_RCSR |= I2S_RCSR_RE; // Enable receiver    

  }else{
    Serial.println("timeout");
  }

  Serial.println(I2S1_RCSR, HEX);
  Serial.println(I2S1_TCSR, HEX);
}

void loop() {
  // put your main code here, to run repeatedly:

}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}