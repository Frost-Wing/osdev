/**
 * @file pc-speaker.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Headers for PC Speaker drivers.
 * @version 0.1
 * @date 2023-11-11
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>
#include <rtc.h>

/**
 * @brief Plays a specific frequency
 * 
 * @param frequency in Hz
 */
void play_sound(int32 frequency);

/**
 * @brief Mutes the PC Speaker temporaily 
 * 
 */
void mute();

/**
 * @brief Does a BEEP in PC Speaker
 * 
 * @param frequency in Hz
 * @param time in Seconds
 */
void beep(int frequency, int time);