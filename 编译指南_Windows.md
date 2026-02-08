# EasyRPG Player Windows 编译指南

本文档记录了在当前 Windows 设备上编译 EasyRPG Player 的完整步骤。

## 环境信息

- **操作系统**: Windows
- **编译器**: Visual Studio 2022 Community
- **CMake 版本**: 4.2.0
- **依赖库来源**: buildscripts 预编译库

## 目录结构

```
C:\Users\旻\Documents\GitHub\
├── Player\              # EasyRPG Player 源码
├── liblcf\             # liblcf 库源码（可选，Player 会自动克隆）
└── buildscripts\       # 构建脚本和预编译依赖库
    └── windows\
        └── vcpkg\      # 预编译的依赖库
```

## 前提条件

以下软件和依赖库已经安装配置完成：

1. **Visual Studio 2022 Community**
   - 包含 C++ 桌面开发工作负载
   - 已安装路径：`C:\Program Files\Microsoft Visual Studio\2022\Community`

2. **CMake 4.2.0**
   - 已添加到系统 PATH

3. **Git**
   - 用于克隆 liblcf 库

4. **预编译依赖库**
   - 位置：`C:\Users\旻\Documents\GitHub\buildscripts\windows\vcpkg`
   - 包含所有必需的第三方库（SDL2, PNG, fmt, Pixman 等）

## 编译步骤

### 1. 配置编译环境

打开命令提示符（CMD）或 PowerShell，导航到 Player 目录：

```cmd
cd C:\Users\旻\Documents\GitHub\Player
```

### 2. 运行 CMake 配置

使用预设配置来配置项目：

```cmd
cmake --preset windows-x64-vs2022-release
```

**说明**：
- 此命令会自动使用 buildscripts 中的预编译依赖库
- 会自动克隆并配置 liblcf 库（如果 `lib/liblcf` 目录不存在）
- 配置文件会生成到 `build/windows-x64-vs2022-release` 目录

**可用的其他预设**：
- `windows-x64-vs2022-debug` - 调试版本
- `windows-x64-vs2022-relwithdebinfo` - 带调试信息的发布版本
- `windows-x86-vs2022-release` - 32位发布版本

### 3. 编译项目

配置完成后，运行编译命令：

```cmd
cmake --build build/windows-x64-vs2022-release --config Release
```

**说明**：
- 编译过程会先编译 liblcf 库，然后编译 Player
- 编译时间取决于电脑性能，通常需要几分钟

### 4. 查找可执行文件

编译成功后，可执行文件位于：

```
C:\Users\旻\Documents\GitHub\Player\build\windows-x64-vs2022-release\Release\Player.exe
```

## 快速重新编译

如果只是修改了源代码，不需要重新配置，直接运行编译命令即可：

```cmd
cd C:\Users\旻\Documents\GitHub\Player
cmake --build build/windows-x64-vs2022-release --config Release
```

## 清理构建

如果需要完全重新编译，删除 build 目录：

```cmd
cd C:\Users\旻\Documents\GitHub\Player
rmdir /S /Q build\windows-x64-vs2022-release
```

然后重新执行配置和编译步骤。

## 配置文件说明

项目使用了自定义的 CMake 预设配置，配置文件位于：

```
builds/cmake/CMakePresetsUser.json
```

**重要配置项**：

1. **vcpkg toolchain 路径**（第30行）：
   ```json
   "toolchainFile": "C:/Users/旻/Documents/GitHub/buildscripts/windows/vcpkg/scripts/buildsystems/vcpkg.cmake"
   ```

2. **自动编译 liblcf**（第32行）：
   ```json
   "PLAYER_BUILD_LIBLCF": "ON"
   ```

这些配置已经设置好，通常不需要修改。

## 常见问题

### 问题1：找不到 vcpkg toolchain 文件

**错误信息**：
```
Could not find toolchain file: "/windows/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

**解决方案**：
检查 `builds/cmake/CMakePresetsUser.json` 文件中的 `toolchainFile` 路径是否正确。

### 问题2：找不到 liblcf

**错误信息**：
```
Could not find a package configuration file provided by "liblcf"
```

**解决方案**：
确保 `CMakePresetsUser.json` 中的 `PLAYER_BUILD_LIBLCF` 设置为 `"ON"`。

### 问题3：编译错误

如果遇到编译错误，尝试：
1. 清理 build 目录后重新编译
2. 确保 Visual Studio 2022 已正确安装
3. 检查是否有未提交的代码修改导致编译失败

## 更新依赖库

如果需要更新 buildscripts 中的预编译依赖库：

```cmd
cd C:\Users\旻\Documents\GitHub\buildscripts\windows
download_prebuilt.cmd
```

**注意**：这会删除现有的 vcpkg 目录并重新下载（约1.4GB）。

## 使用 Visual Studio IDE 编译

如果更喜欢使用 Visual Studio IDE：

1. 配置项目（同上）
2. 打开解决方案文件：
   ```
   build\windows-x64-vs2022-release\EasyRPG_Player.sln
   ```
3. 在 Visual Studio 中选择 Release 配置
4. 点击"生成" → "生成解决方案"

## 参考文档

- [官方编译文档](docs/BUILDING.md)
- [buildscripts Windows 说明](../buildscripts/windows/README.md)
- [EasyRPG 官网](https://easyrpg.org/)

---

**最后更新**: 2026-02-08
**编译器版本**: MSVC 19.44.35222.0 (Visual Studio 2022)
