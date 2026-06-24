# 文件类型介绍

## 概述

每种文件类型都有独特的二进制签名（Magic Bytes），用于识别文件格式。数据恢复软件通过识别这些签名来定位和恢复文件。

## 文件签名基础

### 什么是 Magic Bytes

Magic Bytes 是文件开头（有时也在结尾）的特定字节序列，用于标识文件类型。

```
文件结构:
┌────────────────┬────────────────┬────────────────┐
│ 文件头 (Header)│ 文件体 (Body)  │ 文件尾 (Footer)│
│ Magic Bytes    │ 实际数据       │ 结束标记       │
└────────────────┴────────────────┴────────────────┘
```

---

## 常见文件类型签名

### 图像文件

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| JPEG | `FF D8 FF E0` | `....` | .jpg, .jpeg |
| JPEG (JFIF) | `FF D8 FF E0 00 10 4A 46 49 46` | `....JFIF` | .jpg |
| JPEG (Exif) | `FF D8 FF E1` | `....` | .jpg |
| PNG | `89 50 4E 47 0D 0A 1A 0A` | `.PNG....` | .png |
| GIF87a | `47 49 46 38 37 61` | `GIF87a` | .gif |
| GIF89a | `47 49 46 38 39 61` | `GIF89a` | .gif |
| BMP | `42 4D` | `BM` | .bmp |
| TIFF (Intel) | `49 49 2A 00` | `II*.` | .tif, .tiff |
| TIFF (Motorola) | `4D 4D 00 2A` | `MM.*` | .tif, .tiff |
| WebP | `52 49 46 46 xx xx xx xx 57 45 42 50` | `RIFF....WEBP` | .webp |
| ICO | `00 00 01 00` | `....` | .ico |
| PSD | `38 42 50 53` | `8BPS` | .psd |
| RAW (Canon) | `49 49 1A 00 00 00 48 45` | `II..HE` | .cr2 |
| RAW (Nikon) | `4D 4D 00 2A` | `MM.*` | .nef |

### 文档文件

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| PDF | `25 50 44 46` | `%PDF` | .pdf |
| DOC/XLS/PPT (OLE) | `D0 CF 11 E0 A1 B1 1A E1` | `........` | .doc, .xls, .ppt |
| DOCX/XLSX/PPTX | `50 4B 03 04` | `PK..` | .docx, .xlsx, .pptx |
| RTF | `7B 5C 72 74 66` | `{\rtf` | .rtf |
| OpenDocument | `50 4B 03 04` | `PK..` | .odt, .ods |
| PostScript | `25 21 50 53` | `%!PS` | .ps |
| EPUB | `50 4B 03 04` | `PK..` | .epub |

### 压缩文件

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| ZIP | `50 4B 03 04` | `PK..` | .zip |
| RAR | `52 61 72 21 1A 07` | `Rar!..` | .rar |
| RAR5 | `52 61 72 21 1A 07 01 00` | `Rar!....` | .rar |
| 7Z | `37 7A BC AF 27 1C` | `7z....` | .7z |
| GZIP | `1F 8B` | `..` | .gz |
| BZIP2 | `42 5A 68` | `BZh` | .bz2 |
| XZ | `FD 37 7A 58 5A 00` | `.7zXZ.` | .xz |
| TAR | `75 73 74 61 72` | `ustar` (at offset 257) | .tar |

### 音频文件

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| MP3 (ID3) | `49 44 33` | `ID3` | .mp3 |
| MP3 (帧同步) | `FF FB` | `..` | .mp3 |
| WAV | `52 49 46 46 xx xx xx xx 57 41 56 45` | `RIFF....WAVE` | .wav |
| FLAC | `66 4C 61 43` | `fLaC` | .flac |
| OGG | `4F 67 67 53` | `OggS` | .ogg |
| MIDI | `4D 54 68 64` | `MThd` | .mid |
| AIFF | `46 4F 52 4D xx xx xx xx 41 49 46 46` | `FORM....AIFF` | .aiff |

### 视频文件

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| AVI | `52 49 46 46 xx xx xx xx 41 56 49 20` | `RIFF....AVI ` | .avi |
| MP4/MOV | `00 00 00 xx 66 74 79 70` | `....ftyp` | .mp4, .mov |
| MKV | `1A 45 DF A3` | `.E..` | .mkv |
| FLV | `46 4C 56` | `FLV` | .flv |
| WMV | `30 26 B2 75 8E 66 CF 11` | `0&.u.f..` | .wmv |
| MPEG | `00 00 01 BA` | `....` | .mpg |
| WebM | `1A 45 DF A3` | `.E..` | .webm |

### 可执行文件

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| PE (Windows) | `4D 5A` | `MZ` | .exe, .dll |
| ELF (Linux) | `7F 45 4C 46` | `.ELF` | (无) |
| Mach-O (macOS) | `FE ED FA CE` | `....` | (无) |
| Mach-O 64 | `FE ED FA CF` | `....` | (无) |
| Java Class | `CA FE BA BE` | `....` | .class |
| .NET DLL | `4D 5A` | `MZ` | .dll |

### 数据库文件

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| SQLite | `53 51 4C 69 74 65 20 66 6F 72 6D 61 74` | `SQLite format` | .db, .sqlite |
| MS Access | `00 01 00 00 53 74 61 6E 64 61 72 64 20 4A 65 74` | `....Standard Jet` | .mdb |

### 其他常见格式

| 格式 | 十六进制签名 | ASCII | 扩展名 |
|------|-------------|-------|--------|
| ISO 9660 | `43 44 30 30 31` | `CD001` (at offset 0x8001) | .iso |
| Torrent | `64 38 3A 61 6E 6E 6F 75 6E 63 65` | `d8:announce` | .torrent |
| Bitcoin Wallet | `00 00 00 00 01 00 00 00 00 00 00 00` | `............` | .dat |

---

## 文件尾部签名

部分文件格式在文件尾部也有结束标记：

| 格式 | 结束标记 | 说明 |
|------|----------|------|
| JPEG | `FF D9` | 图像结束 |
| PNG | `49 45 4E 44 AE 42 60 82` | IEND 块 |
| GIF | `3B` | GIF Trailer |
| ZIP | `50 4B 05 06` | End of Central Directory |

---

## 文件雕刻 (File Carving)

### 原理

通过扫描磁盘上的文件签名，定位文件起始和结束位置，提取完整文件。

```
磁盘扫描过程:
┌─────────────────────────────────────────────────────────┐
│ 扇区 1 │ 扇区 2 │ 扇区 3 │ 扇区 4 │ 扇区 5 │ ...      │
└─────────────────────────────────────────────────────────┘
         ↓
    识别到 JPEG 签名 (FF D8 FF E0)
         ↓
    继续扫描直到找到 JPEG 结束标记 (FF D9)
         ↓
    提取区间数据为 .jpg 文件
```

### 挑战

1. **文件碎片化**: 文件可能分散在磁盘不同位置
2. **误识别**: 签名可能出现在数据中而非文件开头
3. **无结束标记**: 部分格式没有明确的结束标记
4. **覆盖写入**: 部分数据已被新数据覆盖

### 恢复策略

```
策略选择:
├── 仅头尾签名 (Header-Footer)
│   └── 适用于: 有明确结束标记的格式 (JPEG, PNG)
├── 固定大小 (Fixed Size)
│   └── 适用于: 固定块大小的格式 (数据库页)
├── 结构感知 (Structure-Aware)
│   └── 适用于: 解析内部结构验证完整性
└── 统计方法 (Statistical)
    └── 适用于: 碎片化文件的智能拼接
```

---

## 签名数据库格式

建议的签名配置文件格式：

```json
{
  "signatures": [
    {
      "name": "JPEG",
      "extensions": ["jpg", "jpeg"],
      "header": "FF D8 FF E0",
      "footer": "FF D9",
      "maxSize": 52428800,
      "category": "image"
    },
    {
      "name": "PNG",
      "extensions": ["png"],
      "header": "89 50 4E 47 0D 0A 1A 0A",
      "footer": "49 45 4E 44 AE 42 60 82",
      "maxSize": 104857600,
      "category": "image"
    }
  ]
}
```

---

## 参考资源

- [File Signatures Database](https://www.garykessler.net/library/file_sigs.html)
- [Wikipedia: List of file signatures](https://en.wikipedia.org/wiki/List_of_file_signatures)
- [Gary Kessler's File Signatures](https://www.garykessler.net/library/file_sigs.html)
