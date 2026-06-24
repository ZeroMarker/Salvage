# 模块设计与开发计划

## 项目概述

Salvage 是一款 Windows 平台数据恢复工具，MVP 阶段聚焦 NTFS 文件系统的已删除文件恢复。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        用户界面层 (CLI)                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │ 扫描命令    │  │ 恢复命令    │  │ 配置/帮助命令           │ │
│  └──────┬──────┘  └──────┬──────┘  └────────────┬────────────┘ │
├─────────┴────────────────┴──────────────────────┴──────────────┤
│                        核心逻辑层                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │ 扫描引擎    │  │ 恢复引擎    │  │ 任务调度器              │ │
│  └──────┬──────┘  └──────┬──────┘  └────────────┬────────────┘ │
├─────────┴────────────────┴──────────────────────┴──────────────┤
│                        解析层                                   │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌─────────────────┐ │
│  │ 分区解析  │ │ NTFS解析  │ │ FAT解析   │ │ 文件签名识别    │ │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └────────┬────────┘ │
├────────┴─────────────┴─────────────┴────────────────┴──────────┤
│                        设备访问层                               │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 磁盘读取接口 (只读)                                     │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                        平台抽象层                               │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Windows API (CreateFile / ReadFile / DeviceIoControl)   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 模块设计

### 1. 设备访问层 (Device Layer)

**职责**: 提供底层磁盘只读访问

**文件结构**
```
src/device/
├── device.h           # 设备接口定义
├── device_win.c       # Windows 实现
└── device_linux.c     # Linux 实现 (开发/测试用)
```

**接口设计**
```c
typedef struct device {
    char name[256];           // 设备名称
    uint64_t total_sectors;   // 总扇区数
    uint32_t sector_size;     // 扇区大小
    HANDLE handle;            // 系统句柄
} device_t;

// 初始化
int device_open(device_t *dev, const char *path);
void device_close(device_t *dev);

// 读取
int device_read_sectors(device_t *dev, uint64_t lba, 
                        uint32_t count, void *buf);

// 信息
int device_get_info(device_t *dev);
```

**MVP 范围**
- 支持物理磁盘 (`\\.\PhysicalDrive0`)
- 支持逻辑分区 (`\\.\C:`)
- 只读模式，禁止写入

---

### 2. 解析层 - 分区解析 (Partition Parser)

**职责**: 识别分区表类型，提取分区信息

**文件结构**
```
src/partition/
├── partition.h        # 分区接口
├── mbr.c              # MBR 解析
├── gpt.c              # GPT 解析
└── detect.c           # 自动检测
```

**数据结构**
```c
typedef struct partition {
    uint64_t start_lba;       // 起始 LBA
    uint64_t size_sectors;    // 大小 (扇区)
    uint8_t  type;            // 分区类型
    uint8_t  fs_type;         // 文件系统类型
    char     name[64];        // 分区名称
    int      index;           // 分区序号
} partition_t;

typedef struct partition_table {
    int          type;        // PT_MBR / PT_GPT
    int          count;       // 分区数量
    partition_t  entries[128]; // 分区条目
} partition_table_t;

// 解析
int pt_parse_mbr(device_t *dev, partition_table_t *table);
int pt_parse_gpt(device_t *dev, partition_table_t *table);
int pt_detect_and_parse(device_t *dev, partition_table_t *table);
```

---

### 3. 解析层 - NTFS 解析 (NTFS Parser)

**职责**: 解析 NTFS 文件系统结构，读取 MFT 记录

**文件结构**
```
src/fs/ntfs/
├── ntfs.h             # NTFS 数据结构定义
├── boot.c             # 引导扇区解析
├── mft.c              # MFT 记录解析
├── attribute.c        # 属性解析
├── data_run.c         # 数据运行解析
└── index.c            # 索引解析 (目录)
```

**数据结构**
```c
// NTFS 引导扇区
typedef struct ntfs_boot {
    uint8_t  oem[8];          // "NTFS    "
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint64_t total_sectors;
    uint64_t mft_lba;
    uint64_t mft_mirror_lba;
    int8_t   clusters_per_mft;
    uint64_t serial_number;
} ntfs_boot_t;

// MFT 记录头
typedef struct mft_record {
    uint8_t  signature[4];    // "FILE"
    uint16_t update_offset;
    uint16_t update_count;
    uint64_t logfile_seq;
    uint16_t sequence_number;
    uint16_t hard_link_count;
    uint16_t attrs_offset;
    uint16_t flags;           // 0x01=使用中, 0x02=目录
    uint32_t bytes_used;
    uint32_t bytes_allocated;
    uint64_t base_mft;
    uint16_t next_attr_id;
} mft_record_t;

// 属性头
typedef struct attr_header {
    uint32_t type;            // 属性类型
    uint32_t length;
    uint8_t  non_resident;
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attr_id;
} attr_header_t;

// 常驻属性
typedef struct attr_resident {
    uint32_t value_length;
    uint16_t value_offset;
} attr_resident_t;

// 非常驻属性
typedef struct attr_non_resident {
    uint64_t lowest_vcn;
    uint64_t highest_vcn;
    uint16_t data_runs_offset;
    uint64_t allocated_size;
    uint64_t actual_size;
    uint64_t initialized_size;
} attr_non_resident_t;

// 文件名属性
typedef struct attr_filename {
    uint64_t parent_ref;
    uint64_t create_time;
    uint64_t modify_time;
    uint64_t mft_time;
    uint64_t access_time;
    uint64_t allocated_size;
    uint64_t real_size;
    uint32_t flags;
    uint8_t  name_length;
    uint8_t  name_type;       // 0=POSIX, 1=Win32, 2=DOS
    uint16_t name[];          // UTF-16LE
} attr_filename_t;

// 接口
int ntfs_read_boot(device_t *dev, uint64_t part_start, ntfs_boot_t *boot);
int ntfs_read_mft_record(device_t *dev, ntfs_boot_t *boot, 
                         uint64_t mft_index, mft_record_t *record);
int ntfs_parse_attributes(mft_record_t *record, ...);
int ntfs_parse_data_runs(const uint8_t *data, uint64_t *vcn, 
                         uint64_t *lcn, uint64_t *length);
```

**MFT 记录号**
| 记录号 | 文件 |
|--------|------|
| 0 | $MFT |
| 1 | $MFTMirr |
| 2 | $LogFile |
| 3 | $Volume |
| 4 | $AttrDef |
| 5 | $Root |
| 6 | $Bitmap |
| 7 | $Boot |
| 8 | $BadClus |
| 9 | $Secure |
| 10-15 | $UpCase 等 |
| 16+ | 用户文件 |

---

### 4. 解析层 - 文件签名识别 (Signature Scanner)

**职责**: 通过 Magic Bytes 识别文件类型

**文件结构**
```
src/signature/
├── signature.h        # 签名接口
├── database.c         # 签名数据库
├── matcher.c          # 签名匹配器
└── signatures.json    # 签名配置文件
```

**数据结构**
```c
typedef struct file_signature {
    char     name[32];        // 文件类型名
    char     extensions[64];  // 扩展名
    uint8_t  header[32];      // 头部签名
    int      header_len;
    uint8_t  footer[32];      // 尾部签名
    int      footer_len;
    uint64_t max_size;        // 最大文件大小
    int      category;        // 分类
} file_signature_t;

typedef struct signature_db {
    int               count;
    file_signature_t  entries[256];
} signature_db_t;

// 接口
int sig_load_database(signature_db_t *db, const char *path);
int sig_match_header(signature_db_t *db, const uint8_t *data, int len);
int sig_match_footer(signature_db_t *db, int sig_index, 
                     const uint8_t *data, int len);
```

---

### 5. 核心逻辑层 - 扫描引擎 (Scan Engine)

**职责**: 协调各解析模块执行扫描

**文件结构**
```
src/core/
├── scanner.h          # 扫描器接口
├── scanner.c          # 扫描器实现
├── result.h           # 扫描结果
└── result.c           # 结果管理
```

**数据结构**
```c
// 扫描模式
typedef enum {
    SCAN_QUICK,        // 快速扫描 (仅 MFT)
    SCAN_DEEP,         // 深度扫描 (MFT + 签名)
    SCAN_SIGNATURE     // 纯签名扫描
} scan_mode_t;

// 扫描结果
typedef struct scan_result {
    uint64_t    file_id;       // 文件 ID / MFT 号
    char        name[256];     // 文件名
    uint64_t    size;          // 文件大小
    uint64_t    create_time;   // 创建时间
    uint64_t    modify_time;   // 修改时间
    uint8_t     fs_type;       // 文件系统类型
    uint8_t     status;        // 恢复状态
    uint64_t    data_lba;      // 数据位置
    char        signature[32]; // 文件签名类型
    float       confidence;    // 恢复信心度
} scan_result_t;

// 扫描任务
typedef struct scan_task {
    device_t          *device;
    partition_t        partition;
    scan_mode_t        mode;
    scan_result_t     *results;
    int                result_count;
    int                result_capacity;
    volatile int       cancelled;
    void (*progress_cb)(int percent, void *user_data);
    void              *user_data;
} scan_task_t;

// 接口
scan_task_t* scan_create(device_t *dev, partition_t *part, scan_mode_t mode);
int scan_start(scan_task_t *task);
int scan_cancel(scan_task_t *task);
void scan_destroy(scan_task_t *task);
```

**扫描流程**
```
快速扫描 (SCAN_QUICK):
┌─────────────────────────────────────────────────────────┐
│ 1. 读取 NTFS 引导扇区获取 MFT 位置                     │
│ 2. 遍历 MFT 记录                                       │
│ 3. 筛选标记为"未使用"但有有效属性的记录                │
│ 4. 提取文件名、大小、时间戳                            │
│ 5. 计算恢复信心度                                      │
└─────────────────────────────────────────────────────────┘

深度扫描 (SCAN_DEEP):
┌─────────────────────────────────────────────────────────┐
│ 1. 执行快速扫描                                        │
│ 2. 对未识别区域进行签名扫描                            │
│ 3. 合并结果，去重                                      │
│ 4. 尝试关联碎片化文件                                  │
└─────────────────────────────────────────────────────────┘
```

---

### 6. 核心逻辑层 - 恢复引擎 (Recovery Engine)

**职责**: 执行文件恢复操作

**文件结构**
```
src/core/
├── recover.h          # 恢复接口
└── recover.c          # 恢复实现
```

**数据结构**
```c
typedef struct recover_task {
    device_t        *device;
    scan_result_t   *result;        // 要恢复的文件
    char             output_path[512]; // 输出路径
    int              overwrite;     // 是否覆盖
    volatile int     cancelled;
    void (*progress_cb)(uint64_t written, uint64_t total, void *user_data);
    void            *user_data;
} recover_task_t;

// 接口
recover_task_t* recover_create(device_t *dev, scan_result_t *result, 
                               const char *output);
int recover_start(recover_task_t *task);
int recover_cancel(recover_task_t *task);
void recover_destroy(recover_task_t *task);
```

**恢复流程**
```
NTFS 文件恢复:
┌─────────────────────────────────────────────────────────┐
│ 1. 读取 MFT 记录                                       │
│ 2. 解析 $DATA 属性                                     │
│ 3. 如果是常驻数据 → 直接提取                           │
│ 4. 如果是非常驻数据 → 解析数据运行                     │
│ 5. 按数据运行读取簇数据                                │
│ 6. 写入输出文件                                        │
└─────────────────────────────────────────────────────────┘

签名恢复:
┌─────────────────────────────────────────────────────────┐
│ 1. 定位文件起始 (Header)                               │
│ 2. 扫描文件结束 (Footer) 或计算大小                    │
│ 3. 读取区间数据                                        │
│ 4. 写入输出文件                                        │
└─────────────────────────────────────────────────────────┘
```

---

### 7. 用户界面层 (CLI)

**职责**: 命令行交互

**文件结构**
```
src/cli/
├── main.c             # 入口
├── cmd.h              # 命令定义
├── cmd_list.c         # 列出分区
├── cmd_scan.c         # 扫描命令
├── cmd_recover.c      # 恢复命令
└── cmd_config.c       # 配置命令
```

**命令设计**
```
salvage list                        # 列出磁盘和分区
salvage scan <device> [options]     # 扫描已删除文件
salvage recover <device> <file>     # 恢复指定文件
salvage config <key> <value>        # 配置选项

扫描选项:
  -m, --mode <quick|deep|signature>  扫描模式
  -t, --type <image|doc|video|all>   文件类型过滤
  -s, --min-size <bytes>             最小文件大小
  -o, --output <path>                输出目录
  -j, --json                         JSON 输出
  -v, --verbose                      详细输出
```

**交互流程**
```
$ salvage scan \\.\PhysicalDrive0 --mode quick

Scanning PhysicalDrive0...
  Partition 1: NTFS (100GB) [C:]

  [████████████████████░░░░░░░░░░] 65% - Found 127 files

  ID    Name              Size      Type    Confidence
  ─────────────────────────────────────────────────────
  1     photo_001.jpg      2.4 MB    JPEG    95%
  2     document.pdf       156 KB    PDF     92%
  3     report.docx        45 KB     DOCX    88%
  ...

Scan complete: 142 files found

$ salvage recover \\.\PhysicalDrive0 1 -o D:\recovered

Recovering photo_001.jpg...
  [██████████████████████████████] 100%
  Saved to: D:\recovered\photo_001.jpg
```

---

### 8. 工具模块 (Utils)

**文件结构**
```
src/utils/
├── log.h              # 日志
├── log.c
├── endian.h           # 字节序转换
├── str.h              # 字符串工具
├── str.c
├── time.h             # 时间转换
├── time.c
└── progress.h         # 进度条
└── progress.c
```

---

## 完整目录结构

```
Salvage/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── docs/
│   ├── filesystem.md
│   ├── filetypes.md
│   ├── partition-sector.md
│   └── design.md
├── include/
│   └── salvage/
│       ├── salvage.h          # 主头文件
│       ├── device.h
│       ├── partition.h
│       ├── ntfs.h
│       ├── signature.h
│       ├── scanner.h
│       └── recover.h
├── src/
│   ├── device/
│   │   ├── device.h
│   │   ├── device_win.c
│   │   └── device_linux.c
│   ├── partition/
│   │   ├── partition.h
│   │   ├── mbr.c
│   │   ├── gpt.c
│   │   └── detect.c
│   ├── fs/
│   │   └── ntfs/
│   │       ├── ntfs.h
│   │       ├── boot.c
│   │       ├── mft.c
│   │       ├── attribute.c
│   │       ├── data_run.c
│   │       └── index.c
│   ├── signature/
│   │   ├── signature.h
│   │   ├── database.c
│   │   ├── matcher.c
│   │   └── signatures.json
│   ├── core/
│   │   ├── scanner.h
│   │   ├── scanner.c
│   │   ├── result.h
│   │   ├── result.c
│   │   ├── recover.h
│   │   └── recover.c
│   ├── cli/
│   │   ├── main.c
│   │   ├── cmd.h
│   │   ├── cmd_list.c
│   │   ├── cmd_scan.c
│   │   └── cmd_recover.c
│   └── utils/
│       ├── log.h / log.c
│       ├── endian.h
│       ├── str.h / str.c
│       ├── time.h / time.c
│       └── progress.h / progress.c
├── tests/
│   ├── test_device.c
│   ├── test_mbr.c
│   ├── test_gpt.c
│   ├── test_ntfs.c
│   ├── test_signature.c
│   └── test_scanner.c
├── data/
│   └── signatures.json
└── scripts/
    ├── build.bat            # Windows 构建
    └── build.sh             # Linux 构建
```

---

## 开发计划

### Phase 1: 基础框架 (Week 1-2)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| 项目初始化 (CMake) | P0 | 0.5 天 |
| 日志模块 | P0 | 0.5 天 |
| 字节序工具 | P0 | 0.5 天 |
| 设备访问层 (Windows) | P0 | 1 天 |
| 设备访问层 (Linux) | P1 | 1 天 |
| 单元测试框架 | P0 | 0.5 天 |

**交付物**: 可读取磁盘扇区的基础框架

---

### Phase 2: 分区解析 (Week 2-3)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| MBR 解析 | P0 | 1 天 |
| GPT 解析 | P0 | 1 天 |
| 分区类型检测 | P0 | 0.5 天 |
| 测试用例 | P0 | 1 天 |

**交付物**: 可识别磁盘分区布局

---

### Phase 3: NTFS 解析 (Week 3-5)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| 引导扇区解析 | P0 | 0.5 天 |
| MFT 记录读取 | P0 | 2 天 |
| 属性解析 (标准信息/文件名) | P0 | 1 天 |
| 数据运行解析 | P0 | 1.5 天 |
| $DATA 属性处理 | P0 | 1 天 |
| 测试用例 | P0 | 2 天 |

**交付物**: 可解析 NTFS 结构，读取已删除文件元数据

---

### Phase 4: 文件签名 (Week 5-6)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| 签名数据库设计 | P0 | 0.5 天 |
| 签名加载与匹配 | P0 | 1 天 |
| 头部/尾部扫描 | P0 | 1 天 |
| 常见格式签名配置 | P0 | 1 天 |
| 测试用例 | P0 | 1 天 |

**交付物**: 可识别常见文件类型

---

### Phase 5: 扫描引擎 (Week 6-7)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| 快速扫描 (MFT 遍历) | P0 | 2 天 |
| 深度扫描 (签名扫描) | P1 | 2 天 |
| 结果管理 | P0 | 1 天 |
| 进度回调 | P0 | 0.5 天 |

**交付物**: 可扫描已删除文件并生成结果列表

---

### Phase 6: 恢复引擎 (Week 7-8)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| MFT 恢复 (常驻) | P0 | 1 天 |
| MFT 恢复 (非常驻) | P0 | 1.5 天 |
| 签名恢复 | P0 | 1 天 |
| 文件写入 | P0 | 0.5 天 |
| 错误处理 | P0 | 1 天 |

**交付物**: 可恢复已删除文件

---

### Phase 7: CLI 界面 (Week 8-9)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| list 命令 | P0 | 0.5 天 |
| scan 命令 | P0 | 1 天 |
| recover 命令 | P0 | 1 天 |
| 参数解析 | P0 | 0.5 天 |
| 进度显示 | P0 | 0.5 天 |
| 错误提示 | P0 | 0.5 天 |

**交付物**: 可用的命令行工具

---

### Phase 8: 测试与优化 (Week 9-10)

| 任务 | 优先级 | 预计时间 |
|------|--------|----------|
| 集成测试 | P0 | 2 天 |
| 边界条件测试 | P0 | 1 天 |
| 性能优化 | P1 | 2 天 |
| 文档完善 | P1 | 1 天 |

**交付物**: 稳定可用的 MVP 版本

---

## 里程碑

```
Week 2  ──→  基础框架完成，可读取扇区
Week 3  ──→  分区解析完成，可识别分区
Week 5  ──→  NTFS 解析完成，可读取 MFT
Week 6  ──→  文件签名识别完成
Week 7  ──→  扫描引擎完成
Week 8  ──→  恢复引擎完成
Week 9  ──→  CLI 界面完成
Week 10 ──→  MVP 发布
```

---

## 技术选型

| 组件 | 选择 | 理由 |
|------|------|------|
| 语言 | C | 底层控制，跨平台潜力 |
| 构建 | CMake | 跨平台构建 |
| 测试 | Unity / Check | 轻量级 C 测试框架 |
| JSON | cJSON | 轻量级 JSON 解析 |
| CLI | getopt | 标准参数解析 |

---

## 风险与应对

| 风险 | 影响 | 应对 |
|------|------|------|
| MFT 记录损坏 | 无法读取文件名 | 降级为签名扫描 |
| 数据运行碎片化 | 无法完整恢复 | 标记为"部分恢复" |
| 大文件处理 | 内存溢出 | 流式读取，分块处理 |
| 权限不足 | 无法访问磁盘 | 提示以管理员运行 |

---

## MVP 交付范围

**包含**
- NTFS 文件系统支持
- 快速扫描 (MFT 遍历)
- 深度扫描 (文件签名)
- 常见文件类型恢复 (图片/文档/视频)
- 命令行界面

**不包含**
- FAT32/exFAT 支持
- 图形界面
- 碎片化文件智能拼接
- 远程恢复
