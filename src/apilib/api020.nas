[FORMAT "WCOFF"]        ; 生成对象文件的模式
[INSTRSET "i486p"]      ; 表示使用486兼容指令模式
[BITS 32]               ; 生成32位模式机器语言
[FILE "api020.nas"]     ; 源文件名信息

        GLOBAL  _api_beep
    
[SECTION .text]

_api_beep:              ; void api_beep(int tone);
        MOV     EDX,20
        MOV     EAX,[ESP+4]     ; tone
        INT     0x40
        RET