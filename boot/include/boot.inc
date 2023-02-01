;---------- loader 和 kernel ------------
LOADER_BASE_ADDR equ 0x900     ;Loader存放内存首址
LOADER_STACK_TOP equ LOADER_BASE_ADDR
LOADER_START_SECTOR equ 0x2     ;Loader存放硬盘扇区
PAGE_DIR_TABLE_POS equ 0x100000     ;页目录存放首址
KERNEL_START_SECTOR equ 0x9         ;Kernel存放硬盘扇区
KERNEL_BIN_BASE_ADDR equ 0x70000    ;Kernel存放内存首址
KERNEL_ENTRY_POINT equ 0xc0001500   ;Kernel程序入口地址
;---------- gdt描述符属性 ---------------
DESC_G_4K equ 1_00000000000000000000000b     ;G位，表示粒度
DESC_D_32 equ  1_0000000000000000000000b     ;D位，表示为32位
DESC_L    equ   0_000000000000000000000b     ;64位代码标记，此处为0即可
DESC_AVL  equ    0_00000000000000000000b     ;CPU不用此位，此位为0
DESC_LIMIT_CODE2 equ 1111_0000000000000000b   ;表示代码段的段界限值第二段
DESC_LIMIT_DATA2 equ DESC_LIMIT_CODE2         ;表示数据段的段界限值第二段
DESC_LIMIT_VIDEO2 equ 0000_0000000000000000b
DESC_P    equ 1_000000000000000b              ;表示该段存在
DESC_DPL_0 equ 00_0000000000000b              ;描述该段特权值
DESC_DPL_1 equ 01_0000000000000b
DESC_DPL_2 equ 10_0000000000000b
DESC_DPL_3 equ 11_0000000000000b
DESC_S_CODE equ 1_000000000000b               ;代码段为非系统段
DESC_S_DATA equ DESC_S_CODE                   ;数据段为非系统段
DESC_S_sys equ 0_000000000000b
DESC_TYPE_CODE equ 1000_00000000b             ;x=1,c=0,r=0,a=0,代码段可执行，非一致性，不可读，已访问位a清0

DESC_TYPE_DATA equ 0010_00000000b             ;x=0,e=0,w=1,a=0,数据段不可执行，向上扩展,可写,已访问位a清0

;---------以下是定义段描述符的高四字节--------------

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_S_CODE + DESC_TYPE_CODE + 0x00

DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_S_DATA + DESC_TYPE_DATA + 0x00

DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b ;这里书上有误，应为0x0b

;----------- 选择子属性 --------------
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b
TI_GDT equ 000b
TI_LDT equ 100b

;------------ 页表以及相关属性 -------------
PG_P equ 1b
PG_RW_R equ 00b
PG_RW_W equ 10b
PG_US_U equ 100b
PG_US_S equ 000b


;----------- ELF文件相关 -----------------
PT_NULL equ 0x0


