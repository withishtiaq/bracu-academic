// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;

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

    uint64_t root_inode;
    uint64_t mtime_epoch;
    uint32_t flags;

    uint32_t checksum;
} superblock_t;
#pragma pack(pop)
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

#pragma pack(push,1)
typedef struct {
    uint16_t mode;
    uint16_t links;
    uint32_t uid;
    uint32_t gid;
    uint64_t size_bytes;
    uint64_t atime, mtime, ctime;
    uint32_t direct[DIRECT_MAX];
    uint32_t reserved_0, reserved_1, reserved_2, proj_id, uid16_gid16;
    uint64_t xattr_ptr;
    uint64_t inode_crc;
} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t inode_no;
    uint8_t  type;
    char     name[58];
    uint8_t  checksum;
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
static int test_bit(const uint8_t* bm, uint64_t idx){
    return (bm[idx>>3] >> (idx & 7u)) & 1u;
}
static void set_bit(uint8_t* bm, uint64_t idx){
    bm[idx >> 3] |= (uint8_t)(1u << (idx & 7u));
}

// find first zero bit (return index or -1)
static int64_t find_first_zero_bit(const uint8_t* bm, uint64_t nbits){
    for(uint64_t i=0;i<nbits;i++){
        if(!test_bit(bm,i)) return (int64_t)i;
    }
    return -1;
}

// ========================== Main ==========================
int main(int argc, char** argv) {
    crc32_init();

    const char* input = NULL;
    const char* output = NULL;
    const char* filepath = NULL;

    for (int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--input") && i+1<argc) input=argv[++i];
        else if(!strcmp(argv[i],"--output") && i+1<argc) output=argv[++i];
        else if(!strcmp(argv[i],"--file") && i+1<argc) filepath=argv[++i];
        else {
            fprintf(stderr,"Usage: %s --input in.img --output out.img --file <file>\n", argv[0]);
            return 1;
        }
    }
    if(!input || !output || !filepath) die("missing required arguments");

    // read whole input image
    FILE* fi = fopen(input, "rb");
    if(!fi){ perror("fopen input"); return 1; }
    fseeko(fi, 0, SEEK_END);
    off_t fsz = ftello(fi);
    if(fsz <= 0){ fclose(fi); die("bad image size"); }
    fseeko(fi, 0, SEEK_SET);

    uint8_t* img = (uint8_t*)malloc((size_t)fsz);
    if(!img){ fclose(fi); die("malloc failed"); }
    if(fread(img, 1, (size_t)fsz, fi) != (size_t)fsz){ fclose(fi); free(img); die("read image failed"); }
    fclose(fi);

    // map structures
    if((size_t)fsz < BS) { free(img); die("image too small"); }
    superblock_t* sb = (superblock_t*)(img + 0*BS);
    if(sb->block_size != BS || sb->magic != 0x4D565346u){
        free(img); die("invalid superblock");
    }
    uint64_t total_blocks = sb->total_blocks;
    if((uint64_t)fsz != total_blocks * BS){
        free(img); die("image size mismatch");
    }

    uint8_t* ibm = img + sb->inode_bitmap_start*BS;
    uint8_t* dbm = img + sb->data_bitmap_start*BS;

    inode_t* itbl = (inode_t*)(img + sb->inode_table_start*BS);
    uint8_t* data_region = img + sb->data_region_start*BS;

    // -------- load file to add --------
    FILE* ff = fopen(filepath,"rb");
    if(!ff){ perror("open file"); free(img); return 1; }
    fseeko(ff, 0, SEEK_END);
    off_t fsz_file = ftello(ff);
    fseeko(ff, 0, SEEK_SET);

    if(fsz_file < 0){ fclose(ff); free(img); die("bad input file"); }
    uint8_t* fbuf = (uint8_t*)malloc((size_t)fsz_file);
    if(!fbuf){ fclose(ff); free(img); die("malloc file buf failed"); }
    if(fsz_file>0 && fread(fbuf,1,(size_t)fsz_file,ff)!=(size_t)fsz_file){
        fclose(ff); free(fbuf); free(img); die("read file failed");
    }
    fclose(ff);

    // -------- allocate inode --------
    uint64_t max_inodes = sb->inode_count;
    int64_t free_ino_idx0 = find_first_zero_bit(ibm, max_inodes); // 0-based index into table; inode no = idx+1
    if(free_ino_idx0 < 0){ free(fbuf); free(img); die("no free inode"); }
    if((uint64_t)free_ino_idx0 == 0){
        // inode #1 is root; ensure it is already allocated; find next free for file
        // If bit 0 was free (shouldn't be), still we will allocate the first free non-zero.
        // move to next free if available
        int64_t next = -1;
        for(uint64_t i=1;i<max_inodes;i++) if(!test_bit(ibm,i)){ next=(int64_t)i; break; }
        if(next<0){ free(fbuf); free(img); die("no free inode (after root)"); }
        free_ino_idx0 = next;
    }
    uint32_t new_inum = (uint32_t)(free_ino_idx0 + 1); // 1-indexed
    set_bit(ibm, (uint64_t)free_ino_idx0);

    // -------- allocate data blocks --------
    uint64_t need_blocks = (fsz_file == 0) ? 0 : ( ((uint64_t)fsz_file + BS - 1) / BS );
    if(need_blocks > DIRECT_MAX){
        free(fbuf); free(img);
        fprintf(stderr,"Warning: file needs %" PRIu64 " blocks (> %d). Cannot add.\n", need_blocks, DIRECT_MAX);
        return 1;
    }

    uint32_t block_abs[DIRECT_MAX]={0};
    uint64_t free_data_blocks = sb->data_region_blocks;

    // find first-fit data blocks
    for(uint64_t i=0, found=0; i<free_data_blocks && found<need_blocks; i++){
        if(!test_bit(dbm, i)){
            set_bit(dbm, i);
            block_abs[found] = (uint32_t)(sb->data_region_start + i);
            found++;
            if(found==need_blocks) break;
        }
    }
    if(need_blocks>0 && block_abs[need_blocks-1]==0){
        free(fbuf); free(img); die("not enough free data blocks");
    }

    // -------- write file data --------
    for(uint64_t i=0;i<need_blocks;i++){
        uint64_t off = (uint64_t)i * BS;
        uint64_t left = (uint64_t)fsz_file - off;
        uint64_t chunk = left > BS ? BS : left;
        uint8_t* dst = img + (uint64_t)block_abs[i]*BS;
        memset(dst, 0, BS);
        if(chunk>0) memcpy(dst, fbuf+off, (size_t)chunk);
    }

    // -------- build file inode --------
    inode_t node = {0};
    node.mode  = 0100000;  // regular file (octal)
    node.links = 1;        // one directory entry
    node.uid=0; node.gid=0;
    node.size_bytes = (uint64_t)fsz_file;
    uint64_t now = (uint64_t)time(NULL);
    node.atime=now; node.mtime=now; node.ctime=now;
    for(int i=0;i<DIRECT_MAX;i++) node.direct[i]=0;
    for(uint64_t i=0;i<need_blocks;i++) node.direct[i]=block_abs[i];
    node.reserved_0=0; node.reserved_1=0; node.reserved_2=0;
    node.proj_id=0; node.uid16_gid16=0; node.xattr_ptr=0;
    inode_crc_finalize(&node);

    // store inode at index free_ino_idx0
    itbl[free_ino_idx0] = node;

    // -------- update root directory --------
    inode_t* root = &itbl[0]; // inode #1
    // load first data block of root
    if(root->direct[0]==0){ free(fbuf); free(img); die("root has no data block"); }
    uint8_t* rootblk = img + (uint64_t)root->direct[0]*BS;

    // find free dirent (inode_no==0)
    dirent64_t* ents = (dirent64_t*)rootblk;
    int placed = 0;
    for(int i=0;i<(BS/sizeof(dirent64_t)); i++){
        if(ents[i].inode_no == 0){
            dirent64_t de = {0};
            de.inode_no = new_inum;
            de.type = 1; // file
            memset(de.name, 0, sizeof(de.name));
            // extract base name from filepath
            const char* name = filepath;
            const char* slash = strrchr(filepath, '/');
            if(slash && slash[1]) name = slash+1;
            strncpy(de.name, name, sizeof(de.name)-1); // truncate if >58
            dirent_checksum_finalize(&de);
            ents[i] = de;
            placed = 1;
            break;
        }
    }
    if(!placed){
        free(fbuf); free(img);
        die("root directory block full (no free dirent slots)");
    }

    // Per project note: increase root links by 1 for new file (though not typical for POSIX)
    root->links += 1;
    root->mtime = now; root->ctime = now;
    inode_crc_finalize(root);

    // update superblock mtime + checksum
    sb->mtime_epoch = now;
    superblock_crc_finalize(sb);

    // -------- write output image --------
    FILE* fo = fopen(output, "wb");
    if(!fo){ perror("fopen output"); free(fbuf); free(img); return 1; }
    size_t wr = fwrite(img, 1, (size_t)fsz, fo);
    if(wr != (size_t)fsz){ perror("fwrite"); fclose(fo); free(fbuf); free(img); return 1; }
    fclose(fo);

    free(fbuf);
    free(img);

    printf("Added file '%s' as inode #%u\n", filepath, new_inum);
    printf("Output image: %s\n", output);
    return 0;
}
