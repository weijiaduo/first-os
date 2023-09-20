[FORMAT "WCOFF"]        ; 生成对象文件的模式
[INSTRSET "i486p"]      ; 表示使用486兼容指令模式
[BITS 32]               ; 生成32位模式机器语言
[FILE "api027.nas"]     ; 源文件名信息

        GLOBAL  _api_getlang
    
[SECTION .text]

_api_getlang:           ; int api_getlang(void);
        MOV     EDX,27
        INT     0x40
        RET
