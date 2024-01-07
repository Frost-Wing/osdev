#include <basics.h>

// void invoke_syscall(int64 num) {
//     asm volatile (
//         "movq %0, %%rax\n\t"
//         "int $0x80\n\t"
//         :
//         : "g" ((int64)num)
//         : "rax"
//     );
// }

// void send_alive_msg(){
//     invoke_syscall(3);
// }

/**
 * @attention Don't rename this function, if you wanted to rename it, u must change the linker also.
 * 
 */
void dw_main(){
    // ! Some thing wrong with syscalls and there fore it is being sent but not the correct RAX value
    // send_alive_msg();
    int k = 0;
    for(int i = 0; i < 10; i++){
        k *= i;
    }
    return; // Safe to do.
}