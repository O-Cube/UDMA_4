//======================================================================================================
// Author  : Obed Oyandut
// Date    : 04.08.2022
// Version : v1
//======================================================================================================
// This program uses TIVA TM4C1294XL Evaluation Board
//======================================================================================================
// This program demonstrates interrupts in microcontroller and particularly in UART.
// Interrupts are desirable in microcontroller designs because they make efficient use of the
// processor. The processor only needs to service ISR from different peripherals. When there are
// ISR the processor can be put to sleep thus saving energy.
// The testing of this code is done with Realterm. The speed of the communication line is 115200MHz
// and a 8N1 format. The debugging is done in code composer studio.
// TIVA TM4C1294XL uses a single channel for both Tx and Rx interrupts. Software has to determine what
// causes the interrupt by using the MIS.
//========================================================================================================
//                     ! IMPORTANT !
// This program runs endless. Stop with the "Red Square Button"
// in Debug Mode (Terminate = CTRL + F2)
//========================================================================================================

#include "inc/tm4c1294ncpdt.h"
#include <stdint.h>
#include <stdio.h>

//========================================================================================================
// COntrol table length
//========================================================================================================

#define LEN 256

//========================================================================================================
// Receive buffer
//========================================================================================================

unsigned char rxBuffer[LEN];

//========================================================================================================
// Control table
//========================================================================================================

unsigned int controlTable[LEN];

//========================================================================================================
// Extract of the raven by allan e. poe
// Text to be transmitted by udma using uart2 tx to realterm.
//========================================================================================================

unsigned char message[] = "Send more message if you can....";

//=========================================================================================
// ISR of UART2:
// When an event happens, the ISR is executed. The processor does not know whether the Rx/Tx
// caused the interrupt. What causes interrupt is determined from MIS. The interrupt is cleared
// using ICR. An interrupt event is produced when Rx FIFO is 3/4 full and when the Tx FIFO is em
// empty. During a single ISR, up to 12 character is read from Rx FIFO or up to 12 characters is
// written into the Tx FIFO.
//==========================================================================================

void UartRxTxHandler(void) {
    if (UART2_MIS_R & 0x01<<16) {
        UART2_ICR_R |= (0x01<<16);
        printf("DMA receive is done...\n");
        rxBuffer[32] = '\0';
        printf("Payload: %s\n", rxBuffer);
    }

    if (UART2_MIS_R & 0x01<<17) {
        UART2_ICR_R |= (0x01<<17);
        printf("DMA transfer is done...\n");
    }
}

//=========================================================================================
// Configuration of UART2:
// Assign clock and wait for uart peripheral to acquire the clock.
// Turn of the uart peripheral for further configuation.
// Assign a speed of 115200bps to uart peripheral
// LCRH:
// word length: 8
// FEN: All FIFOs enabled
// STP: 1 stop bit
// EPS: Reset value
// PEN: Parity disabled
// BRK: No line break
//==========================================================================================

void configUart2(void) {
    SYSCTL_RCGCUART_R |= (1<<2);
    while((SYSCTL_PRUART_R & (1<<2))==0);
    UART2_CTL_R &= ~(1<<0);
    UART2_IBRD_R = 8;
    UART2_FBRD_R = 44;
    UART2_LCRH_R = 0x00000070;
    UART2_DMACTL_R |= 0x03;
    UART2_CTL_R |= 0x311; // CTS is enabled

    //===========================================================================================
    // Configuration UART2 interrupts:
    // IM:
    // DMATXIM = 1 => DMATXRIS in UARTRIS is masked.
    // DMARXIM = 1 => DMARXRIS in UARTRIS is masked.
    // TXIM = 1 => TXRIS in UARTRIS is masked.
    // RXIM = 1 => RXRIS in UARTRIS is masked
    // EN1:
    // Mask interrupt request to NVIC
    // Re enabe uart peripheral after configuration
    // CTL:
    // RXE set => rx enable
    // TXE set => tx enable
    // UARTEN set
    // =================================================================================================

    UART2_IM_R |= 0x30030;
    NVIC_EN1_R |= (0x1<<1);
   // UART2_IFLS_R |= 0x18; // rx is 3/4 full and tx 3/4 empty
    UART2_CTL_R |= 0x301; // CTS is enabled
}

//==========================================================================================
// Configuration of port D for UART:
// Assign clock to PD. Wait for clock to become stable using
// a blocking while loop. Set alternate select of P(4) and PD(5). Set alternate function
// of PD(4) and PD(5)to UART.
//==========================================================================================

void configPortD(void) {
    SYSCTL_RCGCGPIO_R |= (1<<3);
    while((SYSCTL_PRGPIO_R & (1<<3)) == 0);
    GPIO_PORTD_AHB_DEN_R |= 0x030;
    GPIO_PORTD_AHB_AFSEL_R |= 0x030;
    GPIO_PORTD_AHB_PCTL_R |= 0x110000;
}

//==============================================================================================
// Configuration UART2 tx on channel 1 on rx on channel 0:
// RCGCDMA:
// Assigns clock to udma
// PRDMA:
// Wait for udma peripheral to acquire clock signal
// CFG:
// Enable udma controller
// PRIOSET:
// Sets priority of channel 1 to high. Leave channel 0 to default priority
// ALTCLR:
// Disables alternate control structure in both channels
// USEBURSTCLR:
// Enables burst transfer in both channel
// CHAP0:
// Select udma source i.e. uart2 tx and uart2 rx
// CTLBASE:
// Assign base control table
// ENASET:
// Enable channel 1 and 2 for use
//=============================================================================================================

void udmaConfig(void) {

    SYSCTL_RCGCDMA_R |= 0x01;
    while(!(SYSCTL_PRDMA_R & 0x01));
    UDMA_CFG_R |= 0x01;
    //UDMA_PRIOSET_R |= 0x02;
    UDMA_ALTCLR_R |= 0x03;
    UDMA_USEBURSTCLR_R |= 0x03; // enables burst on this channel
    UDMA_REQMASKCLR_R |= 0x03;
    UDMA_CHMAP0_R |=0x11;
    UDMA_CTLBASE_R = (unsigned int)controlTable;
    UDMA_ENASET_R = 0x03;
}

//===============================================================================================
// COnfiguarion of base control table
//===============================================================================================

void baseTableConfig(void) {

    //==============================================================================================
    // Control structure of channel 0
    // Control word:
    // DSTINC = byte
    // DSTSIZE = byte
    // SRCINC = no increment
    // SRCSIZE = byte
    // ARBSIZE = 4
    // XFERSIZE = 31
    // XFERMODE = auto
    //===============================================================================================

    controlTable[0] = (unsigned int)&UART2_DR_R;
    controlTable[1] = (unsigned int)&rxBuffer[31];
    controlTable[2] = 0x0C0081F2;

    //==============================================================================================
    // Control structure of channel 1
    // Control word:
    // DSTINC = no increment
    // DSTSIZE = byte
    // SRCINC = byte
    // SRCSIZE = byte
    // ARBSIZE = 4
    // XFERSIZE = 31
    // XFERMODE = auto
    //===============================================================================================

    controlTable[4] = (unsigned int)&message[31];
    controlTable[5] = (unsigned int)&UART2_DR_R;
    controlTable[6] = 0xC00081F2;

}



void main(void) {

    configUart2();
    configPortD();
    baseTableConfig();
    udmaConfig();
    UART2_DR_R = '>';

    while(1)
    {

        //========================================================================================================
        // Infinite loop. Processor waits for evvents to occur. When these events occur, the processor goes into
        // an interrupt service routine.
        //========================================================================================================

    }
}
