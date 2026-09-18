# 火眼项目 - 环境搭建指南

> 本文档指导您从零搭建 openvela 开发环境，适用于 Windows + WSL Ubuntu 22.04

## 一、前置要求

- Windows 10/11 (64位)
- 至少 50GB 可用磁盘空间
- 稳定的网络连接（需要下载约 20GB 源码）

## 二、安装 WSL Ubuntu 22.04

### 方法一：通过 Microsoft Store 安装（推荐）

1. 打开 Microsoft Store，搜索 "Ubuntu 22.04 LTS"
2. 点击 "获取" 安装
3. 安装完成后，从开始菜单打开 Ubuntu
4. 首次运行会要求设置用户名和密码

### 方法二：通过命令行安装

以管理员身份打开 PowerShell：

```powershell
# 启用 WSL 功能
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart

# 启用虚拟机平台
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart

# 重启电脑后，设置 WSL 2 为默认版本
wsl --set-default-version 2

# 安装 Ubuntu 22.04
wsl --install -d Ubuntu-22.04
```

## 三、配置 WSL Ubuntu 环境

### 3.1 更新系统

```bash
sudo apt update && sudo apt upgrade -y
```

### 3.2 安装必要工具

```bash
sudo apt install -y \
    git \
    git-lfs \
    curl \
    wget \
    python3 \
    python3-pip \
    build-essential \
    libncurses5-dev \
    libncursesw5-dev \
    flex \
    bison \
    gperf \
    automake \
    autoconf \
    libtool \
    pkg-config \
    cmake \
    ninja-build \
    srecord \
    u-boot-tools \
    device-tree-compiler \
    genromfs \
    xxd \
    unzip \
    zip \
    gzip \
    bzip2 \
    xz-utils \
    cpio \
    rsync \
    file \
    jq \
    xmlstarlet \
    libxml2-utils \
    python3-venv \
    python3-dev \
    python3-setuptools \
    python3-wheel
```

### 3.3 安装 repo 工具

```bash
# 创建 bin 目录
mkdir -p ~/.bin

# 下载 repo
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/.bin/repo
chmod a+x ~/.bin/repo

# 添加到 PATH
echo 'export PATH="$HOME/.bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 验证安装
repo version
```

### 3.4 配置 Git

```bash
# 设置 Git 用户信息（使用您的 GitHub 账号）
git config --global user.name "liu369369"
git config --global user.email "your-email@example.com"

# 生成 SSH 密钥（如果还没有）
ssh-keygen -t ed25519 -C "your-email@example.com"

# 显示公钥，添加到 GitHub
cat ~/.ssh/id_ed25519.pub
```

将显示的公钥添加到 GitHub: Settings → SSH and GPG keys → New SSH key

### 3.5 配置 GitHub CLI（可选）

```bash
# 安装 GitHub CLI
sudo apt install gh

# 登录
gh auth login
```

## 四、拉取 openvela 源码

### 4.1 创建工作目录

```bash
# 建议放在 WSL 文件系统内（性能更好）
mkdir -p ~/openvela-contest
cd ~/openvela-contest
```

### 4.2 初始化 repo

```bash
repo init -u https://github.com/open-vela/contest2026_443_fengxieke.git \
    -b dev-ai-contest-2026 \
    -m contest2026_443_fengxieke.xml
```

### 4.3 同步源码

```bash
# 并行下载，8个线程
repo sync -c -j8
```

> ⚠️ 首次同步需要下载约 20GB 数据，请耐心等待

### 4.4 验证目录结构

```bash
ls -la
# 应该看到类似结构：
# apps/
# nuttx/
# packages/
# build/
# contest2026_443_fengxieke/
# ...
```

## 五、编译 GD32F470V-START 配置

### 5.1 查找板级配置

```bash
# 查找 GD32 相关的配置
find . -iname "*gd32*" -type d

# 或者
ls vendor/gigadevice/boards/
```

### 5.2 编译

```bash
# 在 openvela 工作区根目录执行
./build.sh <board-config-path> [-j8]

# 例如（具体路径以实际为准）：
# ./build.sh vendor/gigadevice/boards/gd32f470v-start/configs/nsh -j8
```

### 5.3 编译成功标志

编译成功后会生成固件文件，通常在 `nuttx.bin` 或类似路径。

## 六、Windows 与 WSL 文件交互

### 6.1 从 Windows 访问 WSL 文件

在文件资源管理器地址栏输入：
```
\\wsl$\Ubuntu-22.04\home\<username>\openvela-contest
```

### 6.2 从 WSL 访问 Windows 文件

```bash
# Windows C 盘挂载在 /mnt/c
ls /mnt/c/Users/刘艳秋/Documents/ChatGPT/openvela
```

## 七、常见问题

### Q1: repo sync 失败

```bash
# 清理后重试
repo forall -c 'git checkout .' 
repo sync -c -j8 --force-sync
```

### Q2: 编译报错缺少依赖

```bash
# 根据错误信息安装对应依赖
sudo apt install <missing-package>
```

### Q3: WSL 内存不足

创建 `%UserProfile%\.wslconfig` 文件：
```ini
[wsl2]
memory=8GB
swap=4GB
```

然后重启 WSL：
```powershell
wsl --shutdown
```

### Q4: 网络问题

如果 GitHub 访问慢，可以配置代理：
```bash
# 在 ~/.bashrc 中添加
export http_proxy="http://127.0.0.1:7890"
export https_proxy="http://127.0.0.1:7890"
```

## 八、下一步

环境搭建完成后，请继续阅读：
- `docs/development_guide.md` - 开发协同指南
- `docs/architecture_design.md` - 应用架构设计（待创建）

---

*最后更新: 2026-09-03*
