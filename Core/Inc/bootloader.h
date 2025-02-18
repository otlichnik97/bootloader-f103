/*
 * bootloader.h
 *
 *  Created on: Feb 18, 2025
 *      Author: shilin
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#define FLASH_APP_ADDRESS  	0x08004000 // сдвиг на 0x4000 (16 кБ)
#define PACKET_SIZE_1024    1024
#define PACKET_SIZE_128    	128
#define YMODEM_SOH			0x01
#define YMODEM_STX			0x02
#define YMODEM_EOT			0x04
#define YMODEM_ACK			0x06
#define YMODEM_NAK			0x15
#define YMODEM_CAN			0x18
#define YMODEM_C_			0x43

// Не является командой протокола YMODEM,
// добавлена конкретно для данного загрузчика
#define YMODEM_R_			0x52

// Инициализация
void BootloaderInit();

// Полная обработка процессов
void BootloaderRun();

// Переход к основной программе
void LaunchApp(void);


#endif /* INC_BOOTLOADER_H_ */
