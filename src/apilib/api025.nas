[FORMAT "WCOFF"]        ; 生成对象文件的模式
[INSTRSET "i486p"]      ; 表示使用486兼容指令模式
[BITS 32]               ; 生成32位模式机器语言
[FILE "api025.nas"]     ; 源文件名信息

        GLOBAL  _api_fread
    
[SECTION .text]

_api_fread:             ; void api_fread(char *buf, int maxsize, int fhandle);
        PUSH    EBX
        MOV     EDX,25
        MOV     EAX,[ESP+16]    ; fhandle
        MOV     ECX,[ESP+12]    ; maxsize
        MOV     EBX,[ESP+8]     ; buf
        INT     0x40
        POP     EBX
        RET