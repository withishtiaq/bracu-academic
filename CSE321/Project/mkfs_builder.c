// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define BS 4096u               // block size
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

// ========================== On-disk structures ==========================

#pragma pack(push, 1)
typedef struct {
    // Superblock fields per spec (little-endian on disk)
    uint32_t magic;               // 0x4D565346 "MVSF"
    uint32_t version;             // 1
    uint32_t block_size;          // 4096

    uint64_t total_blocks;
    uint64_t inode_count;

    uint64_t inode_bitmap_start;
    uint64_t inode_bitmap_blocks;

    uint64_t data_bitmap_start;
    uint64_t data_bitmap_blocks;

    uint64_t inode_table_start;
    uint64_t inode_table_blocks;

    uint64_t data_region_start;
    uint64_t data_region_blocks;

    uint64_t root_inode;          // 1

    uint64_t mtime_epoch;         // build time

    uint32_t flags;               // 0

    // THIS FIELD SHOULD STAY AT THE END
    uint32_t checksum;            // crc32(superblock[0..4091])
} superblock_t;
#pragma pack(pop)
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

#pragma pack(push,1)
typedef struct {
    // inode (120-byte header + 8-byte crc = 128)
    uint16_t mode;        // dir=040000, file=010000 (octal)
    uint16_t links;       // root starts with 2 (., ..)

    uint32_t uid;         // 0
    uint32_t gid;         // 0

    uint64_t size_bytes;

    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;

    uint32_t direct[DIRECT_MAX]; // absolute block numbers (0=unused)

    uint32_t reserved_0;  // 0
    uint32_t reserved_1;  // 0
    uint32_t reserved_2;  // 0
    uint32_t proj_id;     // set to 0 or group id if desired
    uint32_t uid16_gid16; // 0
    uint64_t xattr_ptr;   // 0

    // THIS FIELD SHOULD STAY AT THE END
    uint64_t inode_crc;   // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0
} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    // dirent64: total 64 bytes
    uint32_t inode_no;     // 0 if free
    uint8_t  type;         // 1=file, 2=dir
    char     name[58];     // zero-padded
    uint8_t  checksum;     // XOR of bytes 0..62
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");

// ========================== CRC helpers (given) =========================
uint32_t CRC32_TAB[256];
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}
void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE]; memcpy(tmp, ino, INODE_SIZE);
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c;
}
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];
    de->checksum = x;
}

// ========================== Utils ==========================
static void die(const char* msg){
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}
static int parse_u64(const char* s, uint64_t* out){
    char* end=NULL;
    errno=0;
    unsigned long long v = strtoull(s, &end, 10);
    if(errno || end==s || *end!='\0') return 0;
    *out = (uint64_t)v;
    return 1;
}
static void set_bit(uint8_t* bm, uint64_t idx){
    bm[idx >> 3] |= (uint8_t)(1u << (idx & 7u));
}

// ========================== Main ==========================
int main(int argc, char** argv) {
    crc32_init();

    const char* image = NULL;
    uint64_t size_kib = 0;
    uint64_t inodes = 0;

    // very simple CLI parsing
    for (int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--image") && i+1<argc) image=argv[++i];
        else if(!strcmp(argv[i],"--size-kib") && i+1<argc) parse_u64(argv[++i], &size_kib);
        else if(!strcmp(argv[i],"--inodes") && i+1<argc) parse_u64(argv[++i], &inodes);
        else {
            fprintf(stderr,"Usage: %s --image out.img --size-kib <180..4096> --inodes <128..512>\n", argv[0]);
            return 1;
        }
    }
    if(!image || !size_kib || !inodes) die("missing required arguments");
    if(size_kib < 180 || size_kib > 4096 || (size_kib % 4)!=0) die("size-kib must be 180..4096 and multiple of 4");
    if(inodes < 128 || inodes > 512) die("inodes must be 128..512");

    uint64_t total_bytes = size_kib * 1024ull;
    uint64_t total_blocks = total_bytes / BS;

    // layout (fixed front matter)
    uint64_t inode_bitmap_start = 1; // block 1
    uint64_t data_bitmap_start  = 2; // block 2
    uint64_t inode_bitmap_blocks = 1;
    uint64_t data_bitmap_blocks  = 1;

    // inode table blocks
    uint64_t itbl_bytes = inodes * INODE_SIZE;
    uint64_t inode_table_blocks = (itbl_bytes + BS - 1) / BS; // ceiling

    uint64_t inode_table_start = 3; // immediately after the two bitmaps
    uint64_t data_region_start = inode_table_start + inode_table_blocks;

    if (data_region_start >= total_blocks) die("image too small for metadata");
    uint64_t data_region_blocks = total_blocks - data_region_start;

    // allocate whole image buffer
    uint8_t* img = (uint8_t*)calloc(1, total_blocks * BS);
    if(!img) die("calloc failed");

    // ---------------- Superblock ----------------
    superblock_t* sb = (superblock_t*)(img + 0*BS);
    sb->magic = 0x4D565346u;            // 'MVSF'
    sb->version = 1;
    sb->block_size = BS;

    sb->total_blocks = total_blocks;
    sb->inode_count  = inodes;

    sb->inode_bitmap_start  = inode_bitmap_start;
    sb->inode_bitmap_blocks = inode_bitmap_blocks;

    sb->data_bitmap_start   = data_bitmap_start;
    sb->data_bitmap_blocks  = data_bitmap_blocks;

    sb->inode_table_start   = inode_table_start;
    sb->inode_table_blocks  = inode_table_blocks;

    sb->data_region_start   = data_region_start;
    sb->data_region_blocks  = data_region_blocks;

    sb->root_inode = ROOT_INO;
    sb->mtime_epoch = (uint64_t)time(NULL);
    sb->flags = 0;
    superblock_crc_finalize(sb);

    // ---------------- Bitmaps ----------------
    // inode bitmap: mark inode #1 allocated (1-indexed)
    uint8_t* ibm = img + inode_bitmap_start*BS;
    memset(ibm, 0, BS);
    set_bit(ibm, 0); // inode #1 -> bit 0

    // data bitmap: first data block (for root dir) allocated
    uint8_t* dbm = img + data_bitmap_start*BS;
    memset(dbm, 0, BS);
    set_bit(dbm, 0); // first block in data region

    // ---------------- Inode Table ----------------
    inode_t* itbl = (inode_t*)(img + inode_table_start*BS);
    memset(itbl, 0, inode_table_blocks*BS);

    inode_t root = {0};
    root.mode  = 040000;   // directory (octal)
    root.links = 2;        // . and ..
    root.uid = 0;
    root.gid = 0;
    root.size_bytes = BS;  // we reserve one block
    uint64_t now = (uint64_t)time(NULL);
    root.atime = now; root.mtime = now; root.ctime = now;
    for (int i=0;i<DIRECT_MAX;i++) root.direct[i]=0;

    // direct[0] holds absolute block nr of first data block in data region
    root.direct[0] = (uint32_t)(data_region_start + 0);

    root.reserved_0=0; root.reserved_1=0; root.reserved_2=0;
    root.proj_id=0; root.uid16_gid16=0; root.xattr_ptr=0;
    inode_crc_finalize(&root);

    // write root at index 0 (inode #1)
    itbl[0] = root;

    // ---------------- Root directory block ----------------
    uint8_t* data_region = img + data_region_start*BS;
    memset(data_region, 0, data_region_blocks*BS);

    dirent64_t de_dot = {0};
    de_dot.inode_no = ROOT_INO;
    de_dot.type = 2; // dir
    memset(de_dot.name, 0, sizeof(de_dot.name));
    strncpy(de_dot.name, ".", sizeof(de_dot.name)-1);
    dirent_checksum_finalize(&de_dot);

    dirent64_t de_dotdot = {0};
    de_dotdot.inode_no = ROOT_INO;
    de_dotdot.type = 2; // dir
    memset(de_dotdot.name, 0, sizeof(de_dotdot.name));
    strncpy(de_dotdot.name, "..", sizeof(de_dotdot.name)-1);
    dirent_checksum_finalize(&de_dotdot);

    memcpy(data_region + 0, &de_dot, sizeof(de_dot));
    memcpy(data_region + 64, &de_dotdot, sizeof(de_dotdot));
    // rest remain zero (free entries)

    // ---------------- Write image to disk ----------------
    FILE* f = fopen(image, "wb");
    if(!f) { perror("fopen"); free(img); return 1; }
    size_t wr = fwrite(img, 1, total_blocks*BS, f);
    if(wr != total_blocks*BS){ perror("fwrite"); fclose(f); free(img); return 1; }
    fclose(f);
    free(img);

    printf("Created MiniVSFS image: %s\n", image);
    printf("Blocks: %" PRIu64 " (size: %" PRIu64 " KiB), Inodes: %" PRIu64 "\n",
           total_blocks, size_kib, inodes);
    printf("Inode table blocks: %" PRIu64 ", Data region starts @ block %" PRIu64 "\n",
           inode_table_blocks, data_region_start);
    return 0;
}
