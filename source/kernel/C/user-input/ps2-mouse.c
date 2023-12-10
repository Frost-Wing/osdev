/**
 * @file ps2-mouse.c
 * @author Poncho (AbsurdPoncho)
 * @brief 
 * @version 0.1
 * @date 2023-12-10
 * 
 */
#include <basics.h>
#include <stdbool.h>
#include <hal.h>
#include <opengl/glbackend.h>

extern int fb_width;
extern int fb_height;

#define PS2Leftbutton 0b00000001
#define PS2Middlebutton 0b00000100
#define PS2Rightbutton 0b00000010
#define PS2XSign 0b00010000
#define PS2YSign 0b00100000
#define PS2XOverflow 0b01000000
#define PS2YOverflow 0b10000000

void MouseWait(){
    int64 timeout = 100000;
    while (timeout--){
        if ((inb(0x64) & 0b10) == 0){
            return;
        }
    }
}

void MouseWaitInput(){
    int64 timeout = 100000;
    while (timeout--){
        if (inb(0x64) & 0b1){
            return;
        }
    }
}

void MouseWrite(int8 value){
    MouseWait();
    outb(0x64, 0xD4);
    MouseWait();
    outb(0x60, value);
}

int8 MouseRead(){
    MouseWaitInput();
    return inb(0x60);
}

int8 MouseCycle = 0;
int8 MousePacket[4];
bool MousePacketReady = false;
uvec2 MousePosition;
uvec2 MousePositionOld;

void HandlePS2Mouse(int8 data){

    ProcessMousePacket();
    static bool skip = true;
    if (skip) { skip = false; return; }

    switch(MouseCycle){
        case 0:
           
            if ((data & 0b00001000) == 0) break;
            MousePacket[0] = data;
            MouseCycle++;
            break;
        case 1:
           
            MousePacket[1] = data;
            MouseCycle++;
            break;
        case 2:
            
            MousePacket[2] = data;
            MousePacketReady = true;
            MouseCycle = 0;
            break;
    }
}

void ProcessMousePacket(){
    if (!MousePacketReady) return;

        bool xNegative, yNegative, xOverflow, yOverflow;

        if (MousePacket[0] & PS2XSign){
            xNegative = true;
        }else xNegative = false;

        if (MousePacket[0] & PS2YSign){
            yNegative = true;
        }else yNegative = false;

        if (MousePacket[0] & PS2XOverflow){
            xOverflow = true;
        }else xOverflow = false;

        if (MousePacket[0] & PS2YOverflow){
            yOverflow = true;
        }else yOverflow = false;

        if (!xNegative){
            MousePosition.x += MousePacket[1];
            if (xOverflow){
                MousePosition.x += 255;
            }
        } else
        {
            MousePacket[1] = 256 - MousePacket[1];
            MousePosition.y -= MousePacket[1];
            if (xOverflow){
                MousePosition.x -= 255;
            }
        }

        if (!yNegative){
            MousePosition.y -= MousePacket[2];
            if (yOverflow){
                MousePosition.y -= 255;
            }
        } else
        {
            MousePacket[2] = 256 - MousePacket[2];
            MousePosition.y += MousePacket[2];
            if (yOverflow){
                MousePosition.y += 255;
            }
        }

        if (MousePosition.x < 0) MousePosition.x = 0;
        if (MousePosition.x > fb_width-1) MousePosition.x = fb_width-1;
        
        if (MousePosition.y < 0) MousePosition.y = 0;
        if (MousePosition.y > fb_height-1) MousePosition.y = fb_height-1;
        
        glDrawLine((uvec2){0,0}, MousePositionOld, 0x000001);        
        glDrawLine((uvec2){0,0}, MousePosition, 0xffffff);

        if (MousePacket[0] & PS2Leftbutton){

        }
        if (MousePacket[0] & PS2Middlebutton){
            
        }
        if (MousePacket[0] & PS2Rightbutton){

        }

        MousePacketReady = false;
        MousePositionOld = MousePosition;
}

void InitPS2Mouse(){

    outb(0x21, 0xf2);
    outb(0xa1, 0xff);

    outb(0x64, 0xA8); //enabling the auxiliary device - mouse

    MouseWait();
    outb(0x64, 0x20); //tells the keyboard controller that we want to send a command to the mouse
    MouseWaitInput();
    int8 status = inb(0x60);
    status |= 0b10;
    MouseWait();
    outb(0x64, 0x60);
    MouseWait();
    outb(0x60, status); // setting the correct bit is the "compaq" status byte

    MouseWrite(0xF6);
    MouseRead();

    MouseWrite(0xF4);
    MouseRead();
}