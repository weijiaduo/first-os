[FORMAT "WCOFF"]        ; 生成对象文件的模式
[INSTRSET "i486p"]      ; 表示使用486兼容指令模式
[BITS 32]               ; 生成32位模式机器语言
[FILE "api023.nas"]     ; 源文件名信息

        GLOBAL  _api_fseek
    
[SECTION .text]

_api_fseek:             ; void api_fseek(int fhandle, int offset, int mode);
        PUSH    EBX
        MOV     EDX,23
        MOV     EAX,[ESP+8]     ; fhandle
        MOV     ECX,[ESP+16]    ; mode
        MOV     EBX,[ESP+12]    ; offset
        INT     0x40
        POP     EBX
        RET