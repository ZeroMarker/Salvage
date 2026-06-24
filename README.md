# Salvage

Windows 数据恢复工具 - MVP 版本

## 功能

- **NTFS 文件恢复** - 通过 MFT 遍历恢复已删除文件
- **文件签名扫描** - 识别 27+ 种文件格式 (JPEG/PNG/PDF/ZIP 等)
- **分区表解析** - 支持 MBR 和 GPT 分区表
- **只读访问** - 安全读取磁盘，避免覆写待恢复数据

## 构建

### 依赖

- CMake 3.14+
- GCC / Clang / MSVC

### Linux

```bash
mkdir build && cd build
cmake ..
make
```

### Windows

```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

## 使用

```bash
# 查看帮助
salvage help

# 列出磁盘和分区
salvage list
salvage list /dev/sda

# 扫描已删除文件
salvage scan \\.\PhysicalDrive0
salvage scan /dev/sda --mode deep
salvage scan \\.\C: --mode signature -o results.json

# 恢复文件
salvage recover \\.\PhysicalDrive0 12345 -o D:\recovered
salvage recover /dev/sda 100 -m quick -o ./output
```

### 扫描模式

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| `quick` | MFT 遍历 | NTFS 快速删除恢复 |
| `deep` | MFT + 签名扫描 | 深度恢复 |
| `signature` | 纯签名扫描 | 文件系统损坏 |

## 项目结构

```
Salvage/
├── CMakeLists.txt
├── docs/                   # 技术文档
├── data/signatures.json    # 文件签名数据库
├── include/salvage/        # 公共头文件
├── src/
│   ├── device/             # 设备访问层
│   ├── partition/          # 分区解析 (MBR/GPT)
│   ├── fs/ntfs/            # NTFS 文件系统解析
│   ├── signature/          # 文件签名识别
│   ├── core/               # 扫描/恢复引擎
│   ├── cli/                # 命令行界面
│   └── utils/              # 工具模块
└── tests/                  # 单元测试
```

## 支持的文件格式

| 类别 | 格式 |
|------|------|
| 图像 | JPEG, PNG, GIF, BMP, TIFF, WebP, PSD |
| 文档 | PDF, DOC/XLS/PPT, DOCX/XLSX/PPTX, RTF |
| 音频 | MP3, WAV, FLAC, OGG |
| 视频 | AVI, MP4, MKV, FLV, WMV |
| 压缩 | ZIP, RAR, 7Z, GZIP |
| 可执行 | EXE/DLL, ELF |
| 数据库 | SQLite |

## 技术文档

- [文件系统介绍](docs/filesystem.md)
- [文件类型签名](docs/filetypes.md)
- [分区扇区结构](docs/partition-sector.md)
- [模块设计与开发计划](docs/design.md)

## 测试

```bash
cd build
ctest --output-on-failure
```

## 限制 (MVP)

- 仅支持 NTFS 文件系统
- 不支持碎片化文件拼接
- 不支持图形界面
- 需要管理员/root 权限访问磁盘

## 许可证

MIT License
