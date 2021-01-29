/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include "math.h"
#include "nokia5110_LCD.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

char RxBUF[200]; // bufor do odbioru
char TxBUF[200]; // bufor do nadawania
uint8_t temp[1]; // zmienna na pojedynczy znak
uint8_t rx_e = 0, rx_f = 0; // wskaźniki w buforze do odbioru
uint8_t tx_e = 0, tx_f = 0; // wskaźniki w buforze do nadawania
uint8_t counter = 0; // wskaźnik dla komendy bufora odbiorczego
bool frameStarted = false; // wskaźnik odczytania początku ramki
bool frameCompleted = false; // wskaźnik kompletności ramki
int dataLength; // długośc pola danych
bool sign0xEAIsRead = false; // wskaźnik odczytania znaku specjalnego
uint8_t error;
char frame[100];
bool crcCorrect; // wskaźnik poprawności kodu kontrolnego
char crc[2]; // kod kontrolny odczytany w ramce
char hello_command[] = "Hello, I am STM32!!! \n\r";

uint32_t value_1 = 0;
uint32_t value_2 = 0;
uint32_t Difference = 0;
uint32_t CPM = 0;
uint8_t Is_First_Captured = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */


// KONFIGURACJA PINÓW NA STMF103RB BY GAWRYŚ:

//WYSWIETLACZ:
//RST --> PA5
//CE  --> PA6
//DC  --> PA7
//Din --> PA
//Clk --> PA


//TIM2_CHANNEL_3:
//





void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim2)
{
	if (htim2->Channel == HAL_TIM_ACTIVE_CHANNEL_3)  // tim 2 i channel 3 jest aktywny
	{
		if (Is_First_Captured==0)  // jeśli flaga jest równa zero to wykonujemy:
		{
			value_1 = HAL_TIM_ReadCapturedValue(htim2, TIM_CHANNEL_3);  // pobieramy pierwszą wartość
			Is_First_Captured =1;  // ustawiamy naszą flagę na 1 (true)
		}

		else if (Is_First_Captured)  // jeśli flaga jest podniesiona to:
		{
			value_2 = HAL_TIM_ReadCapturedValue(htim2, TIM_CHANNEL_3);  // pobieramy drugą wartość:

			if (value_2 > value_1)
			{
				Difference = value_2-value_1;   // Wyliczenie różnicy
			}

			else if (value_2 < value_1)
			{
				Difference = ((0xffff-value_1)+value_2) +1; // to samo tylko jeśli value_1 jest większy od 0xffff to nasz prescaler (maksymalna wartość)
			}

			else
			{
				Error_Handler(); //jeśli coś innego to error
			}

			CPM = HAL_RCC_GetPCLK1Freq()*Difference;  // liczenie wartości CPM
			Is_First_Captured = 0;  // resetowanie flagi naszej.
		}
	}
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) { // odbiór i zapis danych na przerwaniach
			if (huart->Instance == USART2) {
				if (rx_e == 199)
					rx_e = 0;
				else
					rx_e++;
				HAL_UART_Receive_IT(&huart2, &RxBUF[rx_e], 1);
	}
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) { // wysyłanie danych na przerwaniach
	if (huart->Instance == USART2) {
		uint8_t temp = TxBUF[tx_f]; // znak do wysłania
		if (tx_f != tx_e) {
			if (tx_f == 199)
				tx_f = 0;
			else
				tx_f++;
			HAL_UART_Transmit_IT(&huart2, &temp, 1);
		}
	}
}

void put(char ch[]) { // dodawanie komend do bufora nadawczego
	uint8_t index = tx_e; // zapamiętanie wartości wskaźnika tx_e
	for (int i = 0; i < strlen(ch); i++) { // dodawanie znaków do bufora
		TxBUF[index] = ch[i];

		if (index == 199)
			index = 0;
		else
			index++;
	}
	__disable_irq();
	if((tx_e == tx_f) && // jeżeli bufor wysyłający był pusty przed dodaniem znaków łańcucha
			(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == SET)) {
		tx_e = index; // przesunięcie wskaźnika na puste miejsce po dodaniu znaków łańcucha
		uint8_t tmp = TxBUF[tx_f]; // zapamiętanie znaku do wysłania
		if (tx_f == 199)
			tx_f = 0;
		else
			tx_f++;
		HAL_UART_Transmit_IT(&huart2, &tmp, 1);
	} else // jeżel w buforze są dane
		tx_e = index; // znaki łańcucha czekają w kolejce w buforze
	__enable_irq();
}
void readChar() {

	if (RxBUF[rx_f] == 0xEE) { // znak początku ramki
		counter = 0; // ustawienie wartości startowych zmiennych
		frameStarted = true;
		frameCompleted = false;
		dataLength = 0;
		sign0xEAIsRead = false;
		for(int i = 0; i < 14; i++){
			frame[i] = 0x00;
		}
		error = 0;
	}

	if (!frameStarted) {
		if (rx_f == 199)
				rx_f = 0;
		else
				rx_f++;
		return;
	}

	frame[counter] = RxBUF[rx_f]; // przepisanie znaku bufora

	if (counter == 5 + dataLength) { // sprawdzenie, czy odczytano ostatni znak ramki
		frameCompleted = true;
	}

	if (!frameCompleted) {
		if (counter == 1) {
			if (frame[1] > 0x08) { // błędna zawartośc pola LENGTH

				error = 0x09;
				my_Error_Handler();
				return;
			}
			else
				dataLength = frame[1];  // długośc pola DATA przed ew. przekodowaniem
		}
		if (counter == 2) {
			char test = frame[2] ^ frame[1];
			if (test != 0xFF)  { // błędna zawartośc pola NOT_LENGTH
				error = 0x08;
				my_Error_Handler();
				return;
			}
		}
		if (dataLength > 0) {
			if (counter > 3 && counter <= counter + dataLength) { // pole DATA
				if (sign0xEAIsRead) { // sprawdzenie czy poprzedni znak to 0xEA
					sign0xEAIsRead = false;
					if (RxBUF[rx_f] == 0xEB)
						frame[counter] = 0xEE; // dekodowanie znaku 0xEE
					else if (RxBUF[rx_f] == 0xEC)
						frame[counter] = 0xEA; // dekodowanie znaku 0xEA
					else {
						error = 0x07;
						my_Error_Handler();
						return;
					}
				}
				if (frame[counter] == 0xEA) {
					sign0xEAIsRead = true;
					counter--;  // w celu przykrycia bieżącego znaku przez następny
				}
			}
		}
		if (counter == 5 + dataLength)
			frameCompleted = true; // przeczytano wszystkie znaki ramki
	}

	if (frameCompleted) {
		checkCRC();
		if (!crcCorrect) {
			error = 0x06;
			my_Error_Handler();
			return;
		} else
			if (frame[3] != 0x11 && frame[3] != 0x22 && frame[3] != 0x33
					&& frame[3] != 0x44 && frame[3] != 0x01 && frame[3] != 0x02
					&& frame[3] != 0x03 && frame[3] != 0x04 && frame[3] != 0x66 && frame[3] != 0x55) {
				error = 0x05;  // nierozpoznane polecenie
				my_Error_Handler();
				return;
			}


		//Wybór komendy oraz obsługa błędu
		switch(frame[3]){ // komendy
					case  0x55:
						put("\nkomenda 0x55");
						LCD_clrScr();
						LCD_print("On latawcem",0,0);
						LCD_print("bialym na",0,1);
						LCD_print("niebie!",0,2);
						LCD_print("Ona plynie",0,3);
						LCD_print("dokola siebie!",0,4);

						break;

					case 0x22:
						put("\nkomenda 0x22");

						LCD_clrScr();
						LCD_print("Milo Mi!",0,0);

						break;
					case 0x11:
						put("\nkomenda 0x11");
						LCD_clrScr();
						LCD_print("Milo Mi!",0,0);

						break;
					case 0x66:
						put("\nkomenda 0x66");
						LCD_clrScr();
						LCD_print("Milo Mi!",0,0);

						break;
					case 0x33:
						put("\nkomenda 0x33");
						break;
		}
	}
	if (rx_f == 199)
			rx_f = 0;
	else
			rx_f++;
	counter++;
}

void checkCRC() {//Obliczanie CRC
	crc[0] = crc[1] = 0x00;
	int frameLength = 4 + dataLength;
 	for(int i = 0; i < frameLength; i++) { // sprawdzanie kolejnych bajtów ramki
 		char byte = frame[i];
 		int numberOf1 = 0;
 		for(int j = 0; j < 8; j++){ // zliczanie jedynek w bitach kolejnego bajtu
 			if (byte & 0x01)
 				numberOf1++;
 			byte >>= 1;
 		}
 		if (numberOf1 == 1 || numberOf1 == 3 || numberOf1 == 5 || numberOf1 == 7)
 			crc[1] |= 0x01; // ustawienie najmłodszego bitu kodu crc

 		crc[0] <<= 1;		// przesunięcie bitowe słowa 16-bitowego
 		if (crc[1] & 0x80)
 			crc[0] |= 0x01;
 		crc[1] <<= 1;
 	}
 	for(int i = 0; i < 15 - frameLength; i++) { // przesunięcie do najstarszego bitu
 		crc[0] <<= 1;
 		if (crc[1] & 0x80)
 			crc[0] |= 0x01;
 		crc[1] <<= 1;
 	}
 	// Porównanie otrzymanego CRC z obliczonym
 	if (frame[counter - 1] == crc[0] && frame[counter] == crc[1])
 		crcCorrect = true;
 	else
 		crcCorrect = false;
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	LCD_setRST(RST_GPIO_Port, RST_Pin);
	LCD_setCE(CE_GPIO_Port, CE_Pin);
	LCD_setDC(DC_GPIO_Port, DC_Pin);
	LCD_setDIN(Din_GPIO_Port, Din_Pin);
	LCD_setCLK(Clk_GPIO_Port, Clk_Pin);
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);
  HAL_UART_Receive_IT(&huart2, &RxBUF[rx_e], 1);
  put("\nWpisz ramke: ");

  LCD_init();

  LCD_print("Wait...",0,0);
  //  LCD_print("bialym na",0,1);
    // LCD_print("niebie!",0,2);
    // LCD_print("Ona plynie",0,3);
   // LCD_print("dokola siebie!",0,4);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */


  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (rx_e != rx_f)
		  readChar();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void my_Error_Handler()
{
	counter = 0;
	frameStarted = false;
	frameCompleted = false;
	dataLength = 0;
	sign0xEAIsRead = false;
	for(int i = 0; i < 14; i++){
		frame[i] = 0x00;
	}
	if (rx_f == 199)
			rx_f = 0;
	else
			rx_f++;

	switch (error) {
	case 0x05: put("\nNierozpoznane polecenie. ");     //EE00FF000000
			   break;
	case 0x06: put("\nBledny kod CRC. ");			   //EE00FF110001
			   break;
	case 0x07: put("\nBledny znak po znaku 0xEA. ");   //EE01FE33EA00
			   break;
	case 0x08: put("\nBledna struktura ramki. ");      //EE0022
			   break;
	case 0x09: put("\nBledna zawartosc pola LENGTH. ");//EEFF00
			   break;							// dobra:EE00FF330000, EE08F73304040404040404046FF0, EE01FE660660
	case 0x0A: put("\nPrzekroczony zakres amplitudy ");
			   break;
	default:   put("\nNierozpoznany kod bledu. ");
			   break;
	}
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
