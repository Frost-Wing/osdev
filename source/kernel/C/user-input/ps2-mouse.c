/**
 * @file ps2-mouse.c
 * @author Poncho (AbsurdPoncho)
 * @brief 
 * @version 0.1
 * @date 2023-12-10
 * 
 */
#include <ps2-mouse.h>

extern int fb_width;
extern int fb_height;

void ps2_mouse_wait(){
    int64 timeout = 100000;
    while (timeout--){
        if ((inb(0x64) & 0b10) == 0){
            return;
        }
    }
}

void ps2_mouse_wait_input(){
    int64 timeout = 100000;
    while (timeout--){
        if (inb(0x64) & 0b1){
            return;
        }
    }
}

void ps2_mouse_write(int8 value){
    ps2_mouse_wait();
    outb(0x64, 0xD4);
    ps2_mouse_wait();
    outb(0x60, value);
}

int8 ps2_mouse_read(){
    ps2_mouse_wait_input();
    return inb(0x60);
}

int8 mouse_cycle = 0;
int8 mouse_packet[4];
bool isMousePacketReady = false;
uvec2 current_mouse_position;
uvec2 previous_mouse_position;

void process_mouse(InterruptFrame* frame){
    int8 data = inb(0x60);
    handle_ps2_mouse(data);
}

void handle_ps2_mouse(int8 data){

    process_mouse_packet();
    static bool skip = true;
    if (skip) {
        skip = false;
        return;
    }

    switch(mouse_cycle){
        case 0:
            if ((data & 0b00001000) == 0) break;
            mouse_packet[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            isMousePacketReady = true;
            mouse_cycle = 0;
            break;
    }
}

void process_mouse_packet(){
    if (!isMousePacketReady) return;

        bool xNegative, yNegative, xOverflow, yOverflow;

        if (mouse_packet[0] & PS2_x){
            xNegative = true;
        }else xNegative = false;

        if (mouse_packet[0] & PS2_y){
            yNegative = true;
        }else yNegative = false;

        if (mouse_packet[0] & PS2_x_overflow){
            xOverflow = true;
        }else xOverflow = false;

        if (mouse_packet[0] & PS2_y_overflow){
            yOverflow = true;
        }else yOverflow = false;

        if (!xNegative){
            current_mouse_position.x += mouse_packet[1];
            if (xOverflow){
                current_mouse_position.x += 255;
            }
        } else
        {
            mouse_packet[1] = 256 - mouse_packet[1];
            current_mouse_position.y -= mouse_packet[1];
            if (xOverflow){
                current_mouse_position.x -= 255;
            }
        }

        if (!yNegative){
            current_mouse_position.y -= mouse_packet[2];
            if (yOverflow){
                current_mouse_position.y -= 255;
            }
        } else
        {
            mouse_packet[2] = 256 - mouse_packet[2];
            current_mouse_position.y += mouse_packet[2];
            if (yOverflow){
                current_mouse_position.y += 255;
            }
        }

        if (current_mouse_position.x < 0) current_mouse_position.x = 0;
        if (current_mouse_position.x > fb_width-1) current_mouse_position.x = fb_width-1;
        
        if (current_mouse_position.y < 0) current_mouse_position.y = 0;
        if (current_mouse_position.y > fb_height-1) current_mouse_position.y = fb_height-1;
        
        glDrawLine((uvec2){0,0}, previous_mouse_position, 0x000000);        
        glDrawLine((uvec2){0,0},  current_mouse_position, 0xffffff);

        if (mouse_packet[0] & PS2_left_button){

        }
        if (mouse_packet[0] & PS2_middle_button){
            
        }
        if (mouse_packet[0] & PS2_right_button){

        }

        isMousePacketReady = false;
        previous_mouse_position = current_mouse_position;
}

void init_ps2_mouse(){
    outb(0x64, 0xA8); //enabling the auxiliary device - mouse

    ps2_mouse_wait();
    outb(0x64, 0x20); //tells the keyboard controller that we want to send a command to the mouse
    ps2_mouse_wait_input();
    int8 status = inb(0x60);
    status |= 0b10;
    ps2_mouse_wait();
    outb(0x64, 0x60);
    ps2_mouse_wait();
    outb(0x60, status); // setting the correct bit is the "compaq" status byte

    ps2_mouse_write(0xF6);
    ps2_mouse_read();

    ps2_mouse_write(0xF4);
    ps2_mouse_read();
}