/**
 * @file pc-speaker.c
 * @author Pradosh (pradoshgame@gmail.com) & OS Dev WIKI (https://wiki.osdev.org/PC_Speaker)
 * @brief The Code for PC Speaker 
 * @version 0.1
 * @date 2023-11-11
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <drivers/pc-speaker.h>

/**
 * @brief Plays a specific frequency
 * 
 * @param frequency in Hz
 */
void play_sound(int32 frequency) {
 	int32 div;
 	int8 tmp;

 	div = 1193180 / frequency;
 	outb(0x43, 0xb6);
 	outb(0x42, (int8) (div) );
 	outb(0x42, (int8) (div >> 8));

 	tmp = inb(0x61);
  	if (tmp != (tmp | 3)) {
 		outb(0x61, tmp | 3);
 	}
 }
 
/**
 * @brief Mutes the PC Speaker temporaily 
 * 
 */
 void mute() {
 	int8 tmp = inb(0x61) & 0xFC;
 
 	outb(0x61, tmp);
 }
 
/**
 * @brief Does a BEEP in PC Speaker
 * 
 * @param frequency in Hz
 * @param time in Seconds
 */
void beep(int frequency, int time) {
    play_sound(frequency);
    sleep(time);
    mute();
}