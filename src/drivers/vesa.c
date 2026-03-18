#include "vesa.h"
#include "font.h"
#include "string.h"
#include "io.h"

static uint32_t* framebuffer;
static uint32_t  screen_width;
static uint32_t  screen_height;
static uint32_t  screen_pitch;
static uint32_t  screen_bpp;
static multiboot_info_t* saved_mbi;

static uint32_t  backbuf_data[800 * 600];
uint32_t* backbuffer = backbuf_data;

static int current_depth = 32;
static monitor_info_t mon;

/* ====== BochsVBE Register Erisimi ====== */
#define VBE_DISPI_INDEX  0x01CE
#define VBE_DISPI_DATA   0x01CF

#define VBE_IDX_ID       0x00
#define VBE_IDX_XRES     0x01
#define VBE_IDX_YRES     0x02
#define VBE_IDX_BPP      0x03
#define VBE_IDX_ENABLE   0x04
#define VBE_IDX_VIRT_W   0x06
#define VBE_IDX_VIRT_H   0x07
#define VBE_IDX_VRAM_64K 0x0A

static uint16_t vbe_dispi_read(uint16_t idx) {
    outw(VBE_DISPI_INDEX, idx);
    return inw(VBE_DISPI_DATA);
}

/* ====== CPUID - VM Algilama ====== */
static int detect_hypervisor(char* hv_name) {
    uint32_t eax, ebx, ecx, edx;
    /* CPUID leaf 1, ECX bit 31 = hypervisor present */
    __asm__ __volatile__("cpuid"
        :"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(1));
    if (!(ecx & (1u << 31))) {
        k_strcpy(hv_name, "Fiziksel Donanim");
        return 0;
    }
    /* CPUID leaf 0x40000000 = hypervisor vendor */
    __asm__ __volatile__("cpuid"
        :"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0x40000000));
    char vendor[13];
    *((uint32_t*)&vendor[0]) = ebx;
    *((uint32_t*)&vendor[4]) = ecx;
    *((uint32_t*)&vendor[8]) = edx;
    vendor[12] = '\0';

    if (k_strncmp(vendor, "VBoxVBoxVBox", 12) == 0)
        k_strcpy(hv_name, "VirtualBox");
    else if (k_strncmp(vendor, "KVMKVMKVM", 9) == 0)
        k_strcpy(hv_name, "KVM/QEMU");
    else if (k_strncmp(vendor, "Microsoft Hv", 12) == 0)
        k_strcpy(hv_name, "Hyper-V");
    else if (k_strncmp(vendor, "VMwareVMware", 12) == 0)
        k_strcpy(hv_name, "VMware");
    else {
        k_strcpy(hv_name, "Bilinmeyen VM");
    }
    return 1;
}

/* ====== Gercek Monitor Algilama ====== */
void vesa_detect_monitor(void) {
    if (mon.detected) return;

    k_memset(&mon, 0, sizeof(monitor_info_t));

    /* 1. Hypervisor / Fiziksel donanim algilama */
    char hv_name[32];
    mon.is_virtual = detect_hypervisor(hv_name);

    /* 2. BochsVBE adapter algilama */
    uint16_t vbe_id = vbe_dispi_read(VBE_IDX_ID);
    int is_bochs = (vbe_id >= 0xB0C0 && vbe_id <= 0xB0CF);

    if (is_bochs) {
        /* BochsVBE/VBoxVGA adaptor */
        if (mon.is_virtual) {
            k_strcpy(mon.adapter_name, hv_name);
            int l = k_strlen(mon.adapter_name);
            k_strcpy(mon.adapter_name + l, " VGA");
        } else {
            k_strcpy(mon.adapter_name, "BochsVBE VGA");
        }

        /* Gercek VRAM miktarini oku */
        uint16_t vram_64k = vbe_dispi_read(VBE_IDX_VRAM_64K);
        mon.vram_bytes = (uint32_t)vram_64k * 65536u;

        /* Gercek cozunurluk */
        mon.hw_width  = vbe_dispi_read(VBE_IDX_XRES);
        mon.hw_height = vbe_dispi_read(VBE_IDX_YRES);
        mon.hw_bpp    = (uint8_t)vbe_dispi_read(VBE_IDX_BPP);
    } else {
        /* Standart VGA veya bilinmeyen */
        k_strcpy(mon.adapter_name, "Standart VGA");
        mon.vram_bytes = 0;
        mon.hw_width  = (uint16_t)screen_width;
        mon.hw_height = (uint16_t)screen_height;
        mon.hw_bpp    = (uint8_t)screen_bpp;
    }

    /* Framebuffer'dan BPP (GRUB'un ayarladigi) */
    if (mon.hw_bpp == 0) mon.hw_bpp = (uint8_t)screen_bpp;
    if (mon.hw_width == 0) mon.hw_width = (uint16_t)screen_width;
    if (mon.hw_height == 0) mon.hw_height = (uint16_t)screen_height;

    mon.max_bpp = mon.hw_bpp;

    /* 3. Multiboot VBE bilgisinden ek veri */
    if (saved_mbi && (saved_mbi->flags & (1u << 11))) {
        /* Framebuffer bilgisi mevcut */
        if (saved_mbi->framebuffer_bpp > mon.max_bpp)
            mon.max_bpp = saved_mbi->framebuffer_bpp;
    }

    /* VRAM 0 ise multiboot'tan tahmin et */
    if (mon.vram_bytes == 0) {
        mon.vram_bytes = screen_width * screen_height * (screen_bpp / 8);
        /* En az 16MB varsay */
        if (mon.vram_bytes < 16 * 1024 * 1024)
            mon.vram_bytes = 16 * 1024 * 1024;
    }

    /* 4. Monitor adi */
    if (mon.is_virtual) {
        k_strcpy(mon.monitor_name, hv_name);
        int l2 = k_strlen(mon.monitor_name);
        k_strcpy(mon.monitor_name + l2, " Virtual Display");
    } else {
        k_strcpy(mon.monitor_name, "PnP Monitor");
    }

    /* 5. Renk derinligi destegi - donanim bazli */
    /* dv[9] = {1,4,8,16,24,30,32,36,48} */
    static const int depths[9] = {1,4,8,16,24,30,32,36,48};

    for (int i = 0; i < 9; i++) {
        int d = depths[i];
        if (d <= 16) {
            /* 1-16 bit: yazilimla simule edilir, her GPU destekler */
            mon.sup_depth[i] = 1;
        } else if (d == 24) {
            /* 24-bit: GPU 24+ bit destekliyorsa */
            mon.sup_depth[i] = (mon.max_bpp >= 24) ? 1 : 0;
        } else if (d == 32) {
            /* 32-bit: GPU 32-bit destekliyorsa */
            mon.sup_depth[i] = (mon.max_bpp >= 32) ? 1 : 0;
        } else if (d == 30) {
            /* 30-bit (10-bit per channel): 10-bit panel gerekir */
            /* Standart 8-bit panel desteklemez */
            /* BochsVBE/VBox 10-bit desteklemez */
            mon.sup_depth[i] = 0;
        } else if (d == 36) {
            /* 36-bit (12-bit per channel): ozel panel */
            mon.sup_depth[i] = 0;
        } else if (d == 48) {
            /* 48-bit (16-bit per channel): profesyonel panel */
            mon.sup_depth[i] = 0;
        }
    }

    /* 6. Yenileme hizi destegi - donanim bazli */
    /* hz_vals[9] = {30,50,60,75,100,120,144,165,240} */
    static const int hz_v[9] = {30,50,60,75,100,120,144,165,240};

    /*
     * VBE/BochsVBE pixel clock hesabi:
     * 800x600 icin toplam: ~1056x628 = 663168 piksel/frame
     * BochsVBE max pixel clock: ~200MHz (sanal)
     * Fiziksel VGA: genellikle 135-250MHz
     *
     * Max Hz = pixel_clock / (htotal * vtotal)
     */
    uint32_t htotal = mon.hw_width + 256;  /* blanking tahmini */
    uint32_t vtotal = mon.hw_height + 28;
    uint32_t pixel_clock;

    if (is_bochs) {
        /* BochsVBE sanal adapter */
        /* VirtualBox: ~200MHz pixel clock destekler */
        /* QEMU: benzer */
        pixel_clock = 200000000u;
    } else {
        /* Standart VGA: 135MHz */
        pixel_clock = 135000000u;
    }

    uint32_t pixels_per_frame = htotal * vtotal;
    if (pixels_per_frame > 0)
        mon.max_hz = (int)(pixel_clock / pixels_per_frame);
    else
        mon.max_hz = 60;

    /* Gercekci sinir */
    if (mon.is_virtual && mon.max_hz > 240)
        mon.max_hz = 240;
    if (!mon.is_virtual && mon.max_hz > 75)
        mon.max_hz = 75;

    for (int i = 0; i < 9; i++) {
        mon.sup_hz[i] = (hz_v[i] <= mon.max_hz) ? 1 : 0;
    }

    mon.detected = 1;
}

monitor_info_t* vesa_get_monitor_info(void) {
    return &mon;
}

/* ====== Init ====== */
void vesa_init(multiboot_info_t* mbi) {
    saved_mbi     = mbi;
    framebuffer   = (uint32_t*)(uint32_t)mbi->framebuffer_addr;
    screen_width  = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    screen_pitch  = mbi->framebuffer_pitch;
    screen_bpp    = mbi->framebuffer_bpp;
    if (screen_width == 0)  screen_width  = 800;
    if (screen_height == 0) screen_height = 600;
    if (screen_bpp == 0)    screen_bpp    = 32;
}

int  vesa_get_width(void)  { return (int)screen_width; }
int  vesa_get_height(void) { return (int)screen_height; }
int  vesa_get_bpp(void)    { return (int)screen_bpp; }
void vesa_set_depth(int d) { current_depth = d; }
int  vesa_get_depth(void)  { return current_depth; }

/* ====== Renk derinligi ====== */
static uint32_t reduce_color(uint32_t color, int depth) {
    if (depth >= 24) return color;
    int r=(color>>16)&0xFF, g=(color>>8)&0xFF, b=color&0xFF;
    switch (depth) {
        case 1: {
            int lum=(r*30+g*59+b*11)/100;
            return lum>105 ? 0xB8C4D0 : 0x0D1520;
        }
        case 4:
            r=((r*3+127)/255)*85; g=((g*3+127)/255)*85; b=((b*3+127)/255)*85;
            break;
        case 8:
            r=((r*7+127)/255)*(255/7); g=((g*7+127)/255)*(255/7); b=((b*3+127)/255)*85;
            break;
        case 16:
            r=((r*31+127)/255)*(255/31); g=((g*63+127)/255)*(255/63); b=((b*31+127)/255)*(255/31);
            break;
    }
    if(r>255)r=255; if(g>255)g=255; if(b>255)b=255;
    return ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}

static uint32_t apply_depth(uint32_t color) { return reduce_color(color, current_depth); }
uint32_t vesa_preview_color(uint32_t color, int depth) { return reduce_color(color, depth); }

/* ====== Hizli piksel islemleri ====== */
void vesa_putpixel_raw(int x, int y, uint32_t color) {
    if (x>=0 && x<(int)screen_width && y>=0 && y<(int)screen_height)
        backbuffer[y*screen_width+x] = color;
}

void vesa_putpixel(int x, int y, uint32_t color) {
    if (x>=0 && x<(int)screen_width && y>=0 && y<(int)screen_height)
        backbuffer[y*screen_width+x] = apply_depth(color);
}

void vesa_fill_rect(int x, int y, int w, int h, uint32_t color) {
    uint32_t c = apply_depth(color);
    int x0=x, x1=x+w, y0=y, y1=y+h;
    if (x0<0) x0=0; if (y0<0) y0=0;
    if (x1>(int)screen_width) x1=(int)screen_width;
    if (y1>(int)screen_height) y1=(int)screen_height;
    int rw = x1-x0;
    if (rw <= 0) return;
    for (int j=y0; j<y1; j++)
        fast_memset32(backbuffer + j*screen_width + x0, c, (uint32_t)rw);
}

void vesa_fill_screen(uint32_t color) {
    uint32_t c = apply_depth(color);
    fast_memset32(backbuffer, c, screen_width * screen_height);
}

void vesa_copy_buffer(void) {
    fast_memcpy32(framebuffer, backbuffer, screen_width * screen_height);
}

/* ====== Yazi cizim ====== */
void vesa_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (c<FONT_FIRST||c>FONT_LAST) c=' ';
    const uint8_t* gl = font_data[c-FONT_FIRST];
    uint32_t fd=apply_depth(fg), bd=apply_depth(bg);
    for (int row=0;row<FONT_HEIGHT;row++) {
        int py=y+row;
        if (py<0||py>=(int)screen_height) continue;
        uint8_t bits=gl[row];
        uint32_t* line = backbuffer + py*screen_width;
        for (int col=0;col<FONT_WIDTH;col++) {
            int px=x+col;
            if (px>=0&&px<(int)screen_width)
                line[px] = (bits&(0x80>>col)) ? fd : bd;
        }
    }
}

void vesa_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    while (*str) {
        if (*str=='\n') { y+=FONT_HEIGHT; x=0; str++; continue; }
        vesa_draw_char(x,y,*str,fg,bg); x+=FONT_WIDTH; str++;
    }
}

void vesa_draw_string_nobg(int x, int y, const char* str, uint32_t fg) {
    uint32_t fd=apply_depth(fg);
    while (*str) {
        if (*str=='\n') { y+=FONT_HEIGHT; x=0; str++; continue; }
        if (*str>=FONT_FIRST && *str<=FONT_LAST) {
            const uint8_t* gl=font_data[*str-FONT_FIRST];
            for (int row=0;row<FONT_HEIGHT;row++) {
                uint8_t bits=gl[row];
                int py=y+row;
                if (py<0||py>=(int)screen_height) continue;
                for (int col=0;col<FONT_WIDTH;col++)
                    if (bits&(0x80>>col)) {
                        int px=x+col;
                        if (px>=0&&px<(int)screen_width)
                            backbuffer[py*screen_width+px]=fd;
                    }
            }
        }
        x+=FONT_WIDTH; str++;
    }
}

void vesa_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    uint32_t c=apply_depth(color);
    for (int i=x;i<x+w;i++) {
        if (i>=0&&i<(int)screen_width) {
            if (y>=0&&y<(int)screen_height) backbuffer[y*screen_width+i]=c;
            if (y+h-1>=0&&y+h-1<(int)screen_height) backbuffer[(y+h-1)*screen_width+i]=c;
        }
    }
    for (int j=y;j<y+h;j++) {
        if (j>=0&&j<(int)screen_height) {
            if (x>=0&&x<(int)screen_width) backbuffer[j*screen_width+x]=c;
            if (x+w-1>=0&&x+w-1<(int)screen_width) backbuffer[j*screen_width+x+w-1]=c;
        }
    }
}

void vesa_draw_rounded_rect(int x, int y, int w, int h, uint32_t color, int r) {
    vesa_fill_rect(x+r,y,w-2*r,h,color);
    vesa_fill_rect(x,y+r,r,h-2*r,color);
    vesa_fill_rect(x+w-r,y+r,r,h-2*r,color);
    for (int cy2=0;cy2<r;cy2++)
        for (int cx=0;cx<r;cx++)
            if (cx*cx+cy2*cy2<=r*r) {
                vesa_putpixel(x+r-cx,y+r-cy2,color);
                vesa_putpixel(x+w-r+cx-1,y+r-cy2,color);
                vesa_putpixel(x+r-cx,y+h-r+cy2-1,color);
                vesa_putpixel(x+w-r+cx-1,y+h-r+cy2-1,color);
            }
}
