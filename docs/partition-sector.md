# 分区扇区介绍

## 概述

磁盘以扇区为最小寻址单位，传统扇区大小为 512 字节，现代硬盘可能使用 4096 字节扇区。分区扇区包含磁盘布局和文件系统的关键元数据。

## 磁盘布局

### 物理结构

```
磁盘物理结构:
├── 盘片 (Platter)
│   ├── 磁面 (Side)
│   │   ├── 磁道 (Track)
│   │   │   ├── 扇区 (Sector) ← 最小寻址单位
│   │   │   └── ...
│   │   └── ...
│   └── ...
└── ...

逻辑结构:
┌─────────────────────────────────────────────────────────┐
│ LBA 0 │ LBA 1 │ LBA 2 │ LBA 3 │ ... │ LBA N           │
│ 扇区0 │ 扇区1 │ 扇区2 │ 扇区3 │     │ 最后扇区        │
└─────────────────────────────────────────────────────────┘
```

### 寻址方式

| 方式 | 说明 | 限制 |
|------|------|------|
| CHS | 柱面-磁头-扇区 | 最大 8GB |
| LBA | 逻辑块地址 | 无限制 |

---

## MBR 分区表 (Master Boot Record)

### 位置

LBA 0 (磁盘第一个扇区)

### 结构

```
MBR 结构 (512 字节):
┌─────────────────────────────────────────────────────────┐
│ 偏移 0x000-0x1BD │ 引导代码 (446 字节)                  │
├─────────────────────────────────────────────────────────┤
│ 偏移 0x1BE-0x1FD │ 分区表 (4 × 16 字节 = 64 字节)      │
├─────────────────────────────────────────────────────────┤
│ 偏移 0x1FE-0x1FF │ 签名 0x55AA (2 字节)                │
└─────────────────────────────────────────────────────────┘
```

### 分区表项结构 (16 字节)

```
偏移   大小   说明
─────────────────────────────────────────────────────
0x00   1      引导标志 (0x80=活动, 0x00=非活动)
0x01   3      CHS 起始地址
0x04   1      分区类型
0x05   3      CHS 结束地址
0x08   4      LBA 起始扇区
0x0C   4      分区大小 (扇区数)
```

### 常见分区类型

| 类型 ID | 说明 |
|---------|------|
| 0x00 | 空 |
| 0x01 | FAT12 |
| 0x04 | FAT16 (< 32MB) |
| 0x05 | 扩展分区 |
| 0x06 | FAT16 (> 32MB) |
| 0x07 | NTFS / exFAT |
| 0x0B | FAT32 (CHS) |
| 0x0C | FAT32 (LBA) |
| 0x0E | FAT16 (LBA) |
| 0x0F | 扩展分区 (LBA) |
| 0x82 | Linux swap |
| 0x83 | Linux |
| 0x85 | Linux 扩展 |
| 0xEE | GPT 保护 |
| 0xEF | EFI 系统分区 |

### MBR 解析代码示例

```c
#pragma pack(push, 1)
typedef struct {
    uint8_t  boot_flag;      // 0x80 = active
    uint8_t  chs_start[3];   // CHS of first sector
    uint8_t  type;           // Partition type
    uint8_t  chs_end[3];     // CHS of last sector
    uint32_t lba_start;      // LBA of first sector
    uint32_t sectors;        // Number of sectors
} MBR_PARTITION_ENTRY;

typedef struct {
    uint8_t             boot_code[446];
    MBR_PARTITION_ENTRY partitions[4];
    uint16_t            signature;  // 0x55AA
} MBR;
#pragma pack(pop)
```

### MBR 限制

- 最大支持 2TB 磁盘
- 最多 4 个主分区
- 扩展分区可包含多个逻辑分区

---

## GPT 分区表 (GUID Partition Table)

### 优势

- 支持超过 2TB 磁盘
- 最多 128 个分区 (默认)
- CRC32 校验
- 备份分区表

### 磁盘布局

```
GPT 磁盘布局:
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ LBA 0    │ LBA 1    │ LBA 2    │ LBA 3-33 │ ...      │ LBA N    │
│ 保护MBR  │ 主GPT头  │ 分区表   │ 分区条目 │ 数据区   │ 备份GPT  │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
```

### GPT 头结构 (LBA 1)

```
偏移   大小   说明
─────────────────────────────────────────────────────
0x00   8      签名 "EFI PART"
0x08   4      版本 (1.0)
0x0C   4      头大小 (通常 92 字节)
0x10   4      头 CRC32
0x14   4      保留
0x18   8      当前 LBA
0x20   8      备份 LBA
0x28   8      第一个可用 LBA
0x30   8      最后一个可用 LBA
0x38   16     磁盘 GUID
0x48   8      分区表起始 LBA
0x50   4      分区表项数量
0x54   4      每个分区表项大小 (通常 128 字节)
0x58   4      分区表 CRC32
```

### GPT 分区表项结构 (128 字节)

```
偏移   大小   说明
─────────────────────────────────────────────────────
0x00   16     分区类型 GUID
0x10   16     分区唯一 GUID
0x20   8      起始 LBA
0x28   8      结束 LBA
0x30   8      属性标志
0x38   72     分区名称 (Unicode)
```

### 常见分区类型 GUID

| GUID | 说明 |
|------|------|
| C12A7328-F81F-11D2-BA4B-00A0C93EC93B | EFI 系统分区 |
| EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 | Microsoft 基本数据 |
| E3C9E316-0B5C-4DB8-817D-F92DF00215AE | Microsoft 保留 |
| DE94BBA4-06D1-4D40-A16A-BFD50179D6AC | Windows 恢复环境 |
| 0FC63DAF-8483-4772-8E79-3D69D8477DE4 | Linux 文件系统 |
| 0657FD6D-A4AB-43C4-84E5-0933C84B4F4F | Linux swap |

### GPT 解析代码示例

```c
#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[8];       // "EFI PART"
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entries_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entries_crc32;
} GPT_HEADER;

typedef struct {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36];           // UTF-16LE
} GPT_PARTITION_ENTRY;
#pragma pack(pop)
```

---

## 扩展分区与逻辑分区

### MBR 扩展分区链

```
主分区 1
主分区 2
主分区 3
扩展分区 ─────────────────────────────────────┐
    │                                          │
    ├── EBR 1 (逻辑分区 1)                     │
    │   ├── 引导记录                           │
    │   ├── 分区表项 (指向逻辑分区)            │
    │   └── 分区表项 (指向下一个 EBR) ─────────┤
    │                                          │
    ├── EBR 2 (逻辑分区 2)                     │
    │   ├── 引导记录                           │
    │   ├── 分区表项 (指向逻辑分区)            │
    │   └── 分区表项 (指向下一个 EBR) ─────────┤
    │                                          │
    └── EBR 3 (逻辑分区 3)                     │
        └── 分区表项 (无下一个 EBR, 0x00) ◄────┘
```

---

## 引导扇区 (Boot Sector)

### FAT32 引导扇区

```
偏移   大小   说明
─────────────────────────────────────────────────────
0x00   3      跳转指令
0x03   8      OEM 名称
0x0B   2      每扇区字节数 (通常 512)
0x0D   1      每簇扇区数
0x0E   2      保留扇区数
0x10   1      FAT 表数量 (通常 2)
0x11   2      根目录项数 (FAT32 为 0)
0x13   2      总扇区数 (< 65536 时使用)
0x15   1      介质类型
0x16   2      每 FAT 扇区数 (FAT32 为 0)
0x18   2      每磁道扇区数
0x1A   2      磁头数
0x1C   4      隐藏扇区数
0x20   4      总扇区数 (大)
0x24   4      每 FAT 扇区数 (FAT32)
0x28   2      标志
0x2A   2      版本
0x2C   4      根目录簇号
0x30   2      FSINFO 扇区号
0x32   2      备份引导扇区号
0x34   12     保留
0x40   1      逻辑驱动器号
0x41   1      保留
0x42   1      扩展引导签名 (0x29)
0x43   4      卷序列号
0x47   11     卷标
0x52   8      文件系统类型 "FAT32   "
0x5A   420    引导代码
0x1FE  2      签名 0x55AA
```

### NTFS 引导扇区

```
偏移   大小   说明
─────────────────────────────────────────────────────
0x00   3      跳转指令
0x03   8      OEM 名称 "NTFS    "
0x0B   2      每扇区字节数
0x0D   1      每簇扇区数
0x0E   2      保留扇区数
0x10   3      未使用
0x13   2      未使用
0x15   1      介质类型
0x16   2      未使用
0x18   2      每磁道扇区数
0x1A   2      磁头数
0x1C   4      隐藏扇区数
0x20   4      未使用
0x24   4      未使用
0x28   8      总扇区数
0x30   8      MFT 起始簇号
0x38   8      MFT 备份起始簇号
0x40   4      每 MFT 记录簇数
0x44   4      每索引簇数
0x48   8      卷序列号
0x50   4      校验和
0x54   426    引导代码
0x1FE  2      签名 0x55AA
```

---

## 数据恢复中的扇区操作

### 关键扇区位置

```
磁盘关键位置:
┌─────────────────────────────────────────────────────────┐
│ LBA 0     │ MBR / 保护 MBR                              │
│ LBA 1     │ GPT 头 (GPT 磁盘)                          │
│ LBA 2-33  │ GPT 分区表条目                              │
│ ...       │                                              │
│ 分区起始  │ 文件系统引导扇区                            │
│ ...       │ MFT / FAT 表                               │
│ ...       │ 数据区                                      │
└─────────────────────────────────────────────────────────┘
```

### 只读访问

数据恢复必须以只读方式访问磁盘，避免覆写待恢复数据。

**Windows API**
```c
// 打开物理磁盘
HANDLE hDisk = CreateFileA(
    "\\\\.\\PhysicalDrive0",
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING,
    NULL
);

// 读取扇区
DWORD bytesRead;
uint8_t sector[512];
ReadFile(hDisk, sector, 512, &bytesRead, NULL);
```

**Linux API**
```c
// 打开磁盘设备
int fd = open("/dev/sda", O_RDONLY);

// 读取扇区
uint8_t sector[512];
lseek(fd, 0, SEEK_SET);
read(fd, sector, 512);
```

### 扇区对齐

```c
// 计算扇区偏移
uint64_t get_byte_offset(uint64_t lba, uint32_t sector_size) {
    return lba * sector_size;
}

// 计算 LBA
uint64_t get_lba(uint64_t byte_offset, uint32_t sector_size) {
    return byte_offset / sector_size;
}
```

---

## 数据恢复流程

```
恢复流程:
┌─────────────────────────────────────────────────────────┐
│ 1. 读取 MBR/GPT 识别分区                               │
├─────────────────────────────────────────────────────────┤
│ 2. 解析分区表确定文件系统类型                          │
├─────────────────────────────────────────────────────────┤
│ 3. 读取引导扇区获取文件系统参数                        │
├─────────────────────────────────────────────────────────┤
│ 4. 定位 MFT/FAT 等元数据结构                           │
├─────────────────────────────────────────────────────────┤
│ 5. 扫描已删除文件记录                                  │
├─────────────────────────────────────────────────────────┤
│ 6. 解析数据运行/簇链定位数据                           │
├─────────────────────────────────────────────────────────┤
│ 7. 提取数据到安全位置                                  │
└─────────────────────────────────────────────────────────┘
```

---

## 参考资料

- Microsoft FAT Specification
- Microsoft NTFS Documentation
- UEFI GPT Specification
- OSDev Wiki - MBR
- OSDev Wiki - GPT
