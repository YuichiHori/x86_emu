#include <stdio.h>
#include <stdint.h> //uint用
#include <stdlib.h> //malloc()用
#include <string.h> //memset()用

/*メモリ1MB*/
#define MEMORY_SIZE (1024 * 1024)

//enum 列挙型
enum Register {EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI, REGISTERS_COUNT };
//配列ポインタ 各文字列の先頭アドレスを格納
char* registers_name[] = {
    "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"
};

typedef struct {   //typedef 新しい型の形 新しい型名
    /*汎用レジスタ*/
    uint32_t registers[REGISTERS_COUNT];

    /*EFLAGSレジスタ*/
    uint32_t eflags;

    /*メモリ(バイト列)*/
    uint8_t* memory;

    /*プログラムカウンタ*/
    uint32_t eip;
} Emulator;
 
/*エミュレータを作成*/
//static関数:他のファイルからの使用を制限
static Emulator* create_emu(size_t size, uint32_t eip, uint32_t esp){
    Emulator* emu = malloc(sizeof(Emulator));
    emu->memory = malloc(size);

    /*汎用レジスタの初期値を全て0にする*/
    memset(emu->registers, 0, sizeof(emu->registers));

    /*レジスタの初期値を代入*/
    emu->eip = eip;
    emu->registers[ESP] = esp;

    return emu;
}

/*エミュレータを破棄*/
static void destroy_emu(Emulator* emu){
    free(emu->memory);
    free(emu);
}

/*汎用レジスタとプログラムカウンタの値を標準出力に出力*/
static void dump_registers(Emulator* emu){
    int i;

    for (i = 0; i < REGISTERS_COUNT; i++){
        printf("%s = %08x\n", registers_name[i], emu->registers[i]);
    }

    printf("EIP = %08x\n", emu->eip);
}

uint8_t get_code8(Emulator* emu, int index){
    return emu->memory[emu->eip + index];
}

int32_t get_sign_code8(Emulator* emu, int index){
    return (int8_t)emu->memory[emu->eip + index];
}

uint32_t get_code32(Emulator* emu, int index){
    int i;
    uint32_t ret = 0;

    /*リトルエディアンでメモリの値を習得する*/
    for (i = 0; i < 4; i++){
        ret |= get_code8(emu, index + i) << (i * 8);
    }
    return ret;
}

int32_t get_sign_code32(Emulator* emu, int index){
    return (int32_t)get_code32(emu, index);
}

//mov命令
void mov_r32_imm32(Emulator* emu){
    uint8_t reg = get_code8(emu, 0) - 0xB8; //レジスタ指定 P47参照
    uint32_t value = get_code32(emu, 1); //値を取り出す
    emu->registers[reg] = value;
    emu->eip += 5;
}

//jmp命令:1ByteのByteの符号付き整数をeipに加算
void short_jump(Emulator* emu){
    int8_t diff = get_sign_code8(emu, 1);
    emu->eip += (diff + 2); //2はjmp命令が2Byteなため
}

void near_jump(Emulator* emu){
    int32_t diff = get_sign_code32(emu, 1);
    emu->eip += (diff + 5);
}

typedef void instruction_func_t(Emulator*); //typedefの説明(https://qiita.com/aminevsky/items/82ecce1d6d8b42d65533)
instruction_func_t* instructions[256];
void init_instruction(void){
    int i;
    memset(instructions, 0, sizeof(instructions));
    for (i = 0; i < 8; i++){
        instructions[0xB8 + i] = mov_r32_imm32;
    }
    instructions[0xE9] = near_jump;
    instructions[0xEB] = short_jump;
}

int main(int argc, char* argv[]){
    FILE* binary; //FILE構造体のポインタ
    Emulator* emu;
    
    if (argc != 2){
        printf("uage: px86 filename\n");
        return 1;
    }

    /*EIPが0、ESPが0x7c00の状態のエミュレータを作る*/
    emu = create_emu(MEMORY_SIZE, 0x7c00, 0x7c00);

    binary = fopen(argv[1], "rb"); //ファイルへのポインタを返す rb:バイナリファイルを読み込みモードでopen
    if(binary == NULL){
        printf("%sファイルが開けません\n", argv[1]);
        return 1;
    }

    /*機械語ファイルを読み込む(最大512バイト)*/
    fread(emu->memory + 0x7c00, 1, 0x200, binary); //1Byte*0x200(512)=512Byte
    fclose(binary);

    init_instruction();

    while (emu->eip < MEMORY_SIZE){
        uint8_t code = get_code8(emu, 0);
        /*現在のプログラムカウンタと実行されるバイナリを出力する*/
        printf("EIP = %X, Code = %02X\n", emu->eip, code);
        
        if(instructions[code] == NULL){
            /*実装されてない命令が来たらVMを終了する*/
            printf("\n\nNot Implemented: %x\n", code);
            break;
        }
        
        /*命令の実行*/
        instructions[code](emu);

        /*EIPが0になったらプログラムを終了*/
        if (emu->eip == 0x00){
            printf("\n\nend of program.\n\n");
            break;
        }
    }

    dump_registers(emu);
    destroy_emu(emu);
    return 0;
}