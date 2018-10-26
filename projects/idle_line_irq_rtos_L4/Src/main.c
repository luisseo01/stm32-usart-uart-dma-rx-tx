/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"
#include "cmsis_os.h"

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USART related functions */
void usart_init(void);
void usart_rx_check(void);
void usart_process_data(const void* data, size_t len);
void usart_send_string(const char* str);

/**
 * \brief           Calculate length of statically allocated array
 */
#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

/**
 * \brief           Buffer for USART DMA
 * \note            Contains RAW unprocessed data received by UART and transfered by DMA
 */
static
uint8_t usart_rx_dma_buffer[64];

/* Thread function entry point */
void init_thread(void const* arg);
void usart_rx_dma_thread(void const* arg);

/* Define thread */
osThreadDef(init, init_thread, osPriorityNormal, 0, 128);
osThreadDef(usart_rx_dma, usart_rx_dma_thread, osPriorityHigh, 0, 128);

/* Message queue ID */
osMessageQId usart_rx_dma_queue_id;

/* Define message queue */
osMessageQDef(usart_rx_dma, 10, sizeof(void *));

/**
 * \brief           Application entry point
 */
int
main(void) {
    /* MCU Configuration--------------------------------------------------------*/

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* Configure the system clock */
    SystemClock_Config();

    /* Create init thread */
    osThreadCreate(osThread(init), NULL);

    /* Start scheduler */
    osKernelStart();

    /* Infinite loop */
    while (1) { }
}

/**
 * \brief           Init thread
 * \param[in]       arg: Thread argument
 */
void
init_thread(void const* arg) {
    /* Initialize all configured peripherals */
    usart_init();

    /* Do other initializations if needed */

    /* Create message queue */
    usart_rx_dma_queue_id = osMessageCreate(osMessageQ(usart_rx_dma), NULL);

    /* Create new thread for USART RX DMA processing */
    osThreadCreate(osThread(usart_rx_dma), NULL);

    /* Terminate this thread */
    osThreadTerminate(NULL);
}

/**
 * \brief           USART DMA check thread
 * \param[in]       arg: Thread argument
 */
void
usart_rx_dma_thread(void const* arg) {
    osEvent evt;

    /* Notify user to start sending data */
    usart_send_string("USART DMA example: DMA HT & TC + USART IDLE LINE IRQ + RTOS processing\r\n");
    usart_send_string("Start sending data to STM32\r\n");

    while (1) {
        /* BLock thread and wait for event to process USART data */
        evt = osMessageGet(usart_rx_dma_queue_id, osWaitForever);

        /* Simply call processing function */
        usart_rx_check();

        (void)evt;
    }
}

/**
 * \brief           Check for new data received with DMA
 */
void
usart_rx_check(void) {
    static size_t old_pos;
    size_t pos;

    /* Calculate current position in buffer */
    pos = ARRAY_LEN(usart_rx_dma_buffer) - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
    if (pos != old_pos) {                       /* Check change in received data */
        if (pos > old_pos) {                    /* Current position is over previous one */
            /* We are in "linear" mode */
            /* Process data directly by subtracting "pointers" */
            usart_process_data(&usart_rx_dma_buffer[old_pos], pos - old_pos);
        } else {
            /* We are in "overflow" mode */
            /* First process data to the end of buffer */
            usart_process_data(&usart_rx_dma_buffer[old_pos], ARRAY_LEN(usart_rx_dma_buffer) - old_pos);
            /* Continue with beginning of buffer */
            usart_process_data(&usart_rx_dma_buffer[0], pos);
        }
    }
    old_pos = pos;                              /* Save current position as old */

    /* Check and manually update if we reached end of buffer */
    if (old_pos == ARRAY_LEN(usart_rx_dma_buffer)) {
        old_pos = 0;
    }
}

/**
 * \brief           Process received data over UART
 * \note            Either process them directly or copy to other bigger buffer
 * \param[in]       data: Data to process
 * \param[in]       len: Length in units of bytes
 */
void
usart_process_data(const void* data, size_t len) {
    const uint8_t* d = data;
    while (len--) {
        LL_USART_TransmitData8(USART2, *d++);
        while (!LL_USART_IsActiveFlag_TXE(USART2));
    }
    while (!LL_USART_IsActiveFlag_TC(USART2));
}

/**
 * \brief           Send string to USART
 * \param[in]       str: String to send
 */
void
usart_send_string(const char* str) {
    usart_process_data(str, strlen(str));
}

/**
 * @brief           USART3 Initialization Function
 */
void
usart_init(void) {
	LL_USART_InitTypeDef USART_InitStruct = {0};
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* Peripheral clock enable */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
	LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

	/**
	 * USART2 GPIO Configuration
	 * PA2   ------> USART2_TX
 	 * PA15  ------> USART2_RX
	 */
	GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
	LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = LL_GPIO_PIN_15;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	GPIO_InitStruct.Alternate = LL_GPIO_AF_3;
	LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* USART2 DMA init */
	LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_6, LL_DMA_REQUEST_2);
	LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_6, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
	LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PRIORITY_LOW);
	LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MODE_CIRCULAR);
	LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MEMORY_INCREMENT);
	LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PDATAALIGN_BYTE);
	LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MDATAALIGN_BYTE);

    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)&USART2->RDR);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)usart_rx_dma_buffer);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_6, ARRAY_LEN(usart_rx_dma_buffer));

    /* Enable HT & TC interrupts */
    LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_6);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_6);

    /* DMA interrupt init */
    NVIC_SetPriority(DMA1_Channel6_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(DMA1_Channel6_IRQn);

	/* USART configuration */
	USART_InitStruct.BaudRate = 115200;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
	LL_USART_Init(USART2, &USART_InitStruct);
	LL_USART_ConfigAsyncMode(USART2);
    LL_USART_EnableDMAReq_RX(USART2);
    LL_USART_EnableIT_IDLE(USART2);

    /* USART interrupt */
    NVIC_SetPriority(USART2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 1));
    NVIC_EnableIRQ(USART2_IRQn);

    /* Enable USART and DMA */
    LL_USART_Enable(USART2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_6);
}


/* Interrupt handlers here */

/**
 * \brief           DMA1 channel6 interrupt handler for USART2 RX
 */
void
DMA1_Channel6_IRQHandler(void) {
    /* Check half-transfer complete interrupt */
    if (LL_DMA_IsEnabledIT_HT(DMA1, LL_DMA_CHANNEL_6) && LL_DMA_IsActiveFlag_HT6(DMA1)) {
        LL_DMA_ClearFlag_HT6(DMA1);             /* CLear half-transfer complete flag */
        osMessagePut(usart_rx_dma_queue_id, 0, 0);  /* Write data to queue. Do not use wait function! */
    }

    /* Check transfer-complete interrupt */
    if (LL_DMA_IsEnabledIT_TC(DMA1, LL_DMA_CHANNEL_6) && LL_DMA_IsActiveFlag_TC6(DMA1)) {
        LL_DMA_ClearFlag_TC6(DMA1);             /* CLear half-transfer complete flag */
        osMessagePut(usart_rx_dma_queue_id, 0, 0);  /* Write data to queue. Do not use wait function! */
    }

    /* Possibly implement other events if needed */
}


/**
 * \brief           USART2 global interrupt handler
 */
void
USART2_IRQHandler(void) {
    /* Check for IDLE line interrupt */
    if (LL_USART_IsEnabledIT_IDLE(USART2) && LL_USART_IsActiveFlag_IDLE(USART2)) {
        LL_USART_ClearFlag_IDLE(USART2);        /* Clear IDLE line flag */
        osMessagePut(usart_rx_dma_queue_id, 0, 0);  /* Write data to queue. Do not use wait function! */
    }

    /* Possibly implement other events if needed */
}

/**
 * \brief           System Clock Configuration
 */
void
SystemClock_Config(void) {
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);

    if (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4) {
        while (1) { }
    }
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_RCC_MSI_Enable();

    /* Wait till MSI is ready */
    while (LL_RCC_MSI_IsReady() != 1) { }
    LL_RCC_MSI_EnableRangeSelection();
    LL_RCC_MSI_SetRange(LL_RCC_MSIRANGE_11);
    LL_RCC_MSI_SetCalibTrimming(0);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_MSI);

    /* Wait till System clock is ready */
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_MSI) { }
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_Init1msTick(48000000);
    LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
    LL_SetSystemCoreClock(48000000);
    LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);
}