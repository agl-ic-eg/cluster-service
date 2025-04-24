#ifndef INCBIN_BODY

#define INCBIN(name, file) \
    extern __attribute__((aligned(16))) const unsigned char incbin_ ## name ## _start[]; \
    extern                              const unsigned char incbin_ ## name ## _end[]
#else
#define STR2(x) #x
#define STR(x) STR2(x)

#define INCBIN_SECTION ".rodata"

#define INCBIN(name, file) \
    __asm__(".section " INCBIN_SECTION "\n" \
            ".global incbin_" STR(name) "_start\n" \
            ".balign 16\n" \
            "incbin_" STR(name) "_start:\n" \
            ".incbin \"" file "\"\n" \
            \
            ".global incbin_" STR(name) "_end\n" \
            ".balign 1\n" \
            "incbin_" STR(name) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern __attribute__((aligned(16))) const char incbin_ ## name ## _start[]; \
    extern                              const char incbin_ ## name ## _end[]
#endif	//#ifndef INCBIN_BODY

INCBIN(alarm0, "sound/alarm0.raw");
INCBIN(alarm1, "sound/alarm1.raw");
INCBIN(alarm2, "sound/alarm2.raw");