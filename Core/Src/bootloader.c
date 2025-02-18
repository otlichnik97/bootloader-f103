/*
 * bootloader.c
 *
 *  Created on: Feb 18, 2025
 *      Author: shilin
 */

#include <string.h>

#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include "bootloader.h"

uint8_t rx_buffer[PACKET_SIZE_1024 + 5]; // Буфер для пакета

// флаги, выставляемые в прерываниях
volatile uint8_t tim2_event_flag = 0;
volatile uint8_t uart2_event_flag = 0;

// счетчик срабатываний таймера
volatile uint32_t tim2_event_counter = 0;

void Flash_Write(uint32_t address, uint8_t *data, uint32_t length) {
    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < length; i += 4) {
        uint32_t word;
        memcpy(&word, &data[i], 4);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word);
    }
    HAL_FLASH_Lock();
}

uint8_t Flash_Erase(uint32_t address, uint32_t size) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = address;
    EraseInitStruct.NbPages = size / FLASH_PAGE_SIZE;
    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
    HAL_FLASH_Lock();
    if (PageError == 0xFFFFFFFF) {
    	return 0;
    } else {
    	return 1;
    }
}

uint16_t Ymodem_CRC16(uint8_t *data, uint16_t length) {
    uint16_t crc = 0x0000;
    int index = 0;
    while (length != 0) {
    	length--;
    	crc ^= (uint16_t)(data[index] << 8);
    	index++;
    	for (int i = 0; i < 8; i++) {
    		if ((crc & 0x8000) != 0) {
    			crc = (uint16_t)((crc << 1) ^ 0x1021);
    		} else {
    			crc <<= 1;
    		}
    	}
    }
    return crc;
}

uint8_t UART_ReceiveByte() {
    uint8_t data;
    HAL_UART_Receive(&huart2, &data, 1, HAL_MAX_DELAY);
    return data;
}

void UART_SendByte(uint8_t data) {
    HAL_UART_Transmit(&huart2, &data, 1, HAL_MAX_DELAY);
}

uint8_t Ymodem_Run() {
    uint32_t flash_address = FLASH_APP_ADDRESS;
    uint8_t header = 0;

    HAL_UART_Receive_IT(&huart2, &header, 1);

    // Цикл проверки наличия новой прошивки.
    // Если на запрос будет положительный ответ, переходим к приему.
    // Если терминал не подключен, выйдем по таймауту.
    while (1) {

    	if (tim2_event_flag) {
    		tim2_event_flag = 0;
    		UART_SendByte(YMODEM_R_);
    	}

    	if (tim2_event_counter > 3) {
    		return 2;
    	}

    	if (uart2_event_flag) {
    		uart2_event_flag = 0;
    		HAL_UART_AbortReceive_IT(&huart2);
        	if (header == YMODEM_ACK) {
        		break;
        	} else if (header == YMODEM_CAN) {
        		return 1;
        	} else {
        		HAL_UART_Receive_IT(&huart2, &header, 1);
        	}
    	}
    }

    NVIC_DisableIRQ(TIM2_IRQn);

    // Очистка памяти (48 кБ)
    if (Flash_Erase(flash_address, 0xC000)) {
    	return 4;
    }

    // Ожидаем передачу файла (посылаем 'C')
    UART_SendByte(YMODEM_C_);

    // Основной цикл приема и записи прошивки
    while (1) {

        header = UART_ReceiveByte();

        if (header == YMODEM_SOH || header == YMODEM_STX) {

            uint16_t packet_size = (header == YMODEM_SOH) ? PACKET_SIZE_128 : PACKET_SIZE_1024;
            HAL_UART_Receive(&huart2, rx_buffer, packet_size + 4, HAL_MAX_DELAY);

            uint8_t packet_num = rx_buffer[0];
            uint8_t packet_num_inv = rx_buffer[1];

            if (packet_num + packet_num_inv != 0xFF) {
                UART_SendByte(YMODEM_NAK);
                continue;
            }

        	uint16_t crc = Ymodem_CRC16(&rx_buffer[2], packet_size);
        	uint16_t crc_real = (rx_buffer[packet_size+2] << 8) | rx_buffer[packet_size+3];

        	// Если CRC-16 верно
        	if (crc == crc_real) {
                if (packet_num == 0) {
                    // Здесь можно добавить проверку имени файла и его размера
                	UART_SendByte(YMODEM_ACK);
                	UART_SendByte('C');
                    continue;
                }
                // Записываем данные во Flash
                Flash_Write(flash_address, &rx_buffer[2], packet_size);
                flash_address += packet_size;
                UART_SendByte(YMODEM_ACK);

            // Если CRC-16 ошибочно
        	} else {
        		UART_SendByte(YMODEM_NAK);
        		continue;
        	}

        }
        else if (header == YMODEM_EOT) {
            UART_SendByte(YMODEM_NAK);
            // Ждём второй EOT
            UART_ReceiveByte();
            UART_SendByte(YMODEM_ACK);
            break;
        }
        else if (header == YMODEM_CAN) {
            return 3; // cancel
        }
    }
    return 0; // OK
}

// Инициализация
void BootloaderInit() {
	// Запуск таймера для отсчета таймаута ожидания прошивки
	HAL_TIM_Base_Start_IT(&htim2);
}

// Полная обработка процессов
void BootloaderRun() {
	int result = 0;
	result = Ymodem_Run();

	// Если прошивка будет принята некорректно, то к основной программе не переходим
	if (result != 0) {
		while(1);
	}
	HAL_UART_MspDeInit(&huart2);
}

// Переход к основной программе
void LaunchApp(void) {
	uint32_t go_address = *((volatile uint32_t*) (FLASH_APP_ADDRESS + 4));
	void (*jump_to_app)(void) = (void*)go_address;
	jump_to_app();
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM2) {
		tim2_event_flag = 1;
		tim2_event_counter++;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART2) {
		uart2_event_flag = 1;
	}
}
