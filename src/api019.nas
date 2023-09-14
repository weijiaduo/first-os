[FORMAT "WCOFF"]        ; 生成对象文件的模式
[INSTRSET "i486p"]      ; 表示使用486兼容指令模式
[BITS 32]               ; 生成32位模式机器语言
[FILE "api019.nas"]     ; 源文件名信息

        GLOBAL  _api_freetimer
    
[SECTION .text]

_api_freetimer:         ; void api_freetimer(int timer);
        PUSH    EBX
        MOV     EDX,19
        MOV     EBX,[ESP+8]     ; timer
        INT     0x40
        POP     EBX
        RET