# llama_gui
🦙 跨平台 Web 图形界面，用于 [llama.cpp](https://github.com/ggml-org/llama.cpp)。零外部依赖（纯 Python 标准库），支持 WSL、纯 Linux。

## 功能一览

| 标签页 | 功能 |
|--------|------|
| 🚀 **运行** | 配置并启动 `llama-cli`（命令行对话）或 `llama-server`（API 服务） |
| 🌐 **Web UI** | 启动 llama-server 后自动打开官方 Web 聊天界面 |
| 📋 **模型管理** | 模型列表的增删改查，GGUF 文件浏览选择 |
| 🔄 **模型转换** | 生成 HF→GGUF 和 LoRA→GGUF 转换命令 |

### 支持的运行参数

- **模式**：命令行对话 / 服务器 API
- **基本参数**：ctx-size（上下文窗口）、ngl（GPU 加速层数）、temperature（温度）、repeat_penalty（重复惩罚）、max_tokens（最大生成长度）、port（端口）
- **选项**：多行输入 (-mli)、MTP 推测解码、自定义额外参数、自定义 llama.cpp 可执行文件路径
- **命令预览**：实时生成完整命令 → 可手动编辑 → 确认执行

## 环境要求

- **Python 3.10+**（只需标准库，无需 pip 安装任何包）
- **llama.cpp** 已编译（CUDA / Vulkan / CPU 均可）
- **浏览器**：Firefox、Chrome、Edge 等现代浏览器

### 可选

- NVIDIA GPU + CUDA 驱动（用于 GPU 加速推理）
- Python + numpy（仅模型转换功能需要）

## 快速开始

```bash
# 一键启动
llamagui

# 或手动启动
cd ~/llama_gui_wsl
python3 app.py --seed    # 首次运行：自动扫描 ~/models/ 中的 .gguf 文件
python3 app.py           # 启动服务 → 浏览器打开 http://localhost:5000
```

---

## WSL 安装指南

### 第一步：编译 llama.cpp（CUDA 版本）

```bash
# 安装编译依赖
sudo pacman -S cmake cuda git make

# 克隆仓库
git clone https://github.com/ggml-org/llama.cpp ~/llama.cpp

# 编译
cd ~/llama.cpp && mkdir -p build && cd build
cmake .. -DGGML_CUDA=ON
cmake --build . --config Release -j$(nproc)
```

编译完成后验证 CUDA 是否可用：

```bash
~/llama.cpp/build/bin/llama-bench -m ~/models/你的模型.gguf -ngl 48
# 输出中应显示 backend = CUDA
```

### 第二步：配置一键启动

```bash
mkdir -p ~/.local/bin
cp ~/llama_gui_wsl/llamagui ~/.local/bin/
chmod +x ~/.local/bin/llamagui
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 第三步：启动

```bash
llamagui
# 自动打开浏览器到 http://localhost:5000
```

### WSL 特别说明

- **浏览器自动打开**：通过 `powershell.exe` 调用 Windows 默认浏览器
- **Windows Terminal 集成**：CLI 模式可一键在 Windows Terminal 中打开命令
- **端口转发**：WSL2 自动将 `localhost` 端口映射到 Windows
- **路径注意**：模型路径使用 Linux 格式（`/home/rhys/models/...`）

### 关于 llama-server 的 404 错误

编译 llama.cpp 时 cmake 会尝试从 HuggingFace 下载内嵌 Web UI 资源。由于国内网络限制，这一步经常超时：

```
-- UI: download dist.tar.gz from b9803 failed: "Timeout was reached"
-- UI: no assets available - building without an embedded UI
```

**解决方案**：从 GitHub Releases 下载预编译的 Web UI 资源包：

```bash
# 下载 UI 资源（替换 b9803 为你的 llama.cpp 版本号）
cd /tmp
curl -L -o llama-ui.tar.gz \
  'https://github.com/ggml-org/llama.cpp/releases/download/b9803/llama-b9803-ui.tar.gz'

# 解压到 llama.cpp 的 UI dist 目录
mkdir -p ~/llama.cpp/tools/ui/dist
cd /tmp && tar xzf llama-ui.tar.gz
cp -r llama-b9803/* ~/llama.cpp/tools/ui/dist/
```

本工具启动 llama-server 时会自动通过 `--path` 参数加载该 UI，浏览器打开后即可使用官方 Web 聊天界面。

---

## 纯 Linux 安装指南

### 第一步：编译 llama.cpp

```bash
# Arch Linux
sudo pacman -S cmake cuda git make

# Ubuntu/Debian
sudo apt install cmake nvidia-cuda-toolkit git make g++
```

后续步骤与 WSL 相同：

```bash
git clone https://github.com/ggml-org/llama.cpp ~/llama.cpp
cd ~/llama.cpp && mkdir -p build && cd build
cmake .. -DGGML_CUDA=ON
cmake --build . --config Release -j$(nproc)
```

### 第二步：配置并启动

```bash
mkdir -p ~/.local/bin
cp ~/llama_gui_wsl/llamagui ~/.local/bin/
chmod +x ~/.local/bin/llamagui
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
llamagui
# 自动通过 xdg-open 打开 Firefox（或系统默认浏览器）
```

### 纯 Linux 与 WSL 的区别

| 功能 | WSL | 纯 Linux |
|------|-----|----------|
| 浏览器打开 | `powershell.exe` | `xdg-open` |
| 终端启动 | Windows Terminal (wt.exe) | 显示命令，手动复制到终端 |
| 其他所有功能 | 完全一致 | 完全一致 |

---

## 使用教程

### 命令行模式（本地对话）

1. 🚀 运行 → 选择「命令行模式」
2. 选择模型（如 gemma4）
3. 调整参数：温度 0.7、ngl 48、ctx-size 65536
4. 可选：勾选「多行输入模式」
5. 可选：勾选「MTP 推测解码」加速推理（需模型支持）
6. 点击「📋 预览命令」查看完整命令
7. 确认无误后点击「▶ 确认运行」
8. 复制命令到终端执行，或点击「在 Windows Terminal 中打开」

### 网页模式（API 服务 + Web UI）

1. 🚀 运行 → 选择「网页模式」
2. 选择模型，设置端口（默认 8081）
3. 点击「▶ 启动 llama-server」
4. 状态指示灯变绿 → 服务器运行中
5. 点击「打开 Web UI」链接，或直接访问 `http://127.0.0.1:8081`
6. 在浏览器中享受类似 ChatGPT 的对话体验

> 需要先下载 Web UI 资源包（见上方「关于 llama-server 的 404 错误」）。

### 管理模型

1. 📋 模型管理 → 左侧列表查看已保存的模型
2. 添加：输入名称 → 浏览选择 .gguf 文件 → 点击添加
3. 编辑：左侧选择模型 → 修改名称或路径 → 点击更新
4. 删除：选择模型 → 点击删除 → 确认

所有操作自动保存。

### 转换模型

1. 🔄 模型转换 → 填写 llama.cpp 仓库路径
2. 填写 HF 模型源文件夹路径
3. 填写输出文件路径（.gguf）
4. 选择输出格式（f16 / q8_0 / q4_K_M 等）
5. 点击「转换模型」或「转换 LoRA」
6. 复制生成的命令到终端执行

> 转换需要 Python 环境和 `convert_hf_to_gguf.py` 脚本。

---

## 目录结构

```
~/llama_gui_wsl/
├── app.py                 # 后端服务（纯 Python stdlib，零依赖）
├── templates/
│   └── index.html         # Web 前端（单页面，4 个标签页）
├── config/
│   ├── config.json        # 运行/转换配置（自动生成）
│   └── models.json        # 模型列表（自动生成）
├── .server.pid            # llama-server 进程 PID（运行时）
├── .server.log            # llama-server 日志（运行时）
├── llamagui               # 一键启动脚本
└── README.md           # 文档
```

---

## 常见问题

### Q: 启动后显示「Server: 未运行」？

正常现象。先到 🚀 运行标签页，选择模型 → 切换到「网页模式」→ 点击「▶ 启动 llama-server」。

### Q: 网页模式打开显示 404？

请确认已下载 Web UI 资源包（见上方「关于 llama-server 的 404 错误」一节）。下载并解压后重启 llama-server 即可。

### Q: 命令行模式和网页模式有什么区别？

| | 命令行模式 | 网页模式 |
|---|---|---|
| 程序 | `llama-cli` | `llama-server` |
| 交互方式 | 终端中对话 | 浏览器 API 调用 |
| 适用场景 | 本地快速测试 | 持续运行的服务 |
| 多客户端 | 不支持 | 支持（OpenAI 兼容 API） |

### Q: 怎样使用 GPU 加速？

将 ngl 设为较大值（如 48 或 99）。需要 CUDA 版本的 llama.cpp。运行 `nvidia-smi` 验证 GPU 可用。

### Q: 模型列表为空？

首次使用会自动扫描 `~/models/` 中的 .gguf 文件。如仍为空：
1. 确认模型在 `~/models/` 中
2. 运行：`python3 ~/llama_gui_wsl/app.py --seed`
3. 或到 📋 模型管理手动添加

### Q: 端口被占用？

修改端口号（如 8082、8083），重新启动服务器。

### Q: 配置文件在哪里？

`~/llama_gui_wsl/config/` — `config.json`（运行设置）、`models.json`（模型列表）。

### Q: MTP 推测解码是什么？

Multi-Token Prediction，预测多个 token 加速生成。需要模型支持（如 Qwen 3.6 系列）。

### Q: 可以从其他设备访问吗？

默认监听 `0.0.0.0`。局域网访问 `http://<宿主机IP>:8081`。注意防火墙。

---

## 项目信息

- **前身**：llama_gui_v2（Windows 原生 C/Win32 程序）
- **llama.cpp 官方**：https://github.com/ggml-org/llama.cpp
- **许可证**：MIT
