#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"
#include "stdint.h"
#define KBD_BUF_PORT 0x60

/* 用转义字符定义一部分控制字符 */
#define esc '\x1b'      //十六进制表示
#define backspace '\b'  
#define tab     '\t'
#define enter '\r'
#define delete '\x7f'

/* 以下不可见字符一律定义为0 */
#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

/* 定义控制字符的通码和断码 */
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make  0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a

/* 定义以下变量来记录相应键是否按下的状态，
 * ext_scancode用于记录makecode是否以0xe0开头 */
static bool ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;

/* 以通码make_code为索引的二维数组 */
static char keymap[][2] = {
  /* 扫描码未与shift组合 */
  /* --------------------------------------------- */
  /* 0x00 */    {0, 0},
  /* 0x01 */    {esc, esc},
  /* 0x02 */    {'1', '!'},
  /* 0x03 */    {'2', '@'},
  /* 0x04 */    {'3', '#'},
  /* 0x05 */    {'4', '$'},
  /* 0x06 */    {'5', '%'},
  /* 0x07 */    {'6', '^'},
  /* 0x08 */    {'7', '&'},
  /* 0x09 */    {'8', '*'},
  /* 0x0A */    {'9', '('},
  /* 0x0B */    {'0', ')'},
  /* 0x0C */    {'-', '_'},
  /* 0x0D */    {'=', '+'},
  /* 0x0E */    {backspace, backspace},
  /* 0x0F */    {tab, tab},
  /* 0x10 */    {'q', 'Q'},
  /* 0x11 */    {'w', 'W'},
  /* 0x12 */    {'e', 'E'},
  /* 0x13 */    {'r', 'R'},
  /* 0x14 */    {'t', 'T'},
  /* 0x15 */    {'y', 'Y'},
  /* 0x16 */    {'u', 'U'},
  /* 0x17 */    {'i', 'I'},
  /* 0x18 */    {'o', 'O'},
  /* 0x19 */    {'p', 'P'},
  /* 0x1A */    {'[', '{'},
  /* 0x1B */    {']', '}'},
  /* 0x1C */    {enter, enter},
  /* 0x1D */    {ctrl_l_char, ctrl_l_char},
  /* 0x1E */    {'a', 'A'},
  /* 0x1F */    {'s', 'S'},
  /* 0x20 */    {'d', 'D'},
  /* 0x21 */    {'f', 'F'},
  /* 0x22 */    {'g', 'G'},
  /* 0x23 */    {'h', 'H'},
  /* 0x24 */    {'j', 'J'},
  /* 0x25 */    {'k', 'K'},
  /* 0x26 */    {'l', 'L'},
  /* 0x27 */    {';', ':'},
  /* 0x28 */    {'\'', '"'},
  /* 0x29 */    {'`', '~'},
  /* 0x2A */    {shift_l_char, shift_l_char},
  /* 0x2B */    {'\\', '|'},
  /* 0x2C */    {'z', 'Z'},
  /* 0x2D */    {'x', 'X'},
  /* 0x2E */    {'c', 'C'},
  /* 0x2F */    {'v', 'V'},
  /* 0x30 */    {'b', 'B'},
  /* 0x31 */    {'n', 'N'},
  /* 0x32 */    {'m', 'M'},
  /* 0x33 */    {',', '<'},
  /* 0x34 */    {'.', '>'},
  /* 0x35 */    {'/', '?'},
  /* 0x36 */    {shift_r_char, shift_r_char},
  /* 0x37 */    {'*', '*'},
  /* 0x38 */    {alt_l_char, alt_l_char},
  /* 0x39 */    {' ', ' '},
  /* 0x3A */    {caps_lock_status, caps_lock_status}
  /* 其他按键暂不处理 */
};

/* 键盘中断处理程序 */
static void intr_keyboard_handler(void){
  uint8_t scancode = inb(KBD_BUF_PORT);
  put_int(scancode);
  return;
}

/* 键盘初始化 */
void keyboard_init(){
  put_str("keyboard init start \n");
  register_handler(0x21, intr_keyboard_handler);
  put_str("keyboard init done \n");
}
