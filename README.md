# 音乐播放器 — Qt 桌面音乐播放器

仿网易云音乐的桌面音乐播放器，基于 Qt6/C++ 构建。

## 已实现的功能



| # | 功能 | 状态 |
|---|------|------|
| 1 | **扫描歌曲 + 持久化播放列表** |  — 递归扫描文件夹（支持 .mp3/.flac/.wav/.ogg/.wma/.m4a/.aac），通过 `Library` 类以 JSON 持久化 |
| 2 | **基本播放控制** |  — 播放/暂停/停止/上一首/下一首，音量滑块，可拖拽进度条，时间显示（mm:ss / mm:ss） |
| 3 | **歌曲信息显示** | — 通过 `QMediaMetaData` 读取标题/歌手/专辑信息，无标签时退回文件名 |
| 4 | **歌词同步 + 时间微调** |  — 自动加载同名 .lrc 文件，解析时间戳，高亮当前行，+/- 0.5s 偏移调整（按歌曲持久化） |
| 5 | **歌词拖动滚动 + 跳转** |  — 鼠标拖拽/滚轮翻阅歌词，居中行显示播放跳转按钮，松手 3 秒无操作自动回到当前播放句 |
| 6 | **播放模式** |  — 列表循环 / 单曲循环 / 随机播放，通过模式按钮切换，工具栏显示当前模式 |
| 7 | **搜索/过滤** |  — 输入关键字实时按标题/歌手/专辑过滤 |
| 8 | **转码（异步）** |  — 基于 QProcess 调用 ffmpeg，不阻塞界面，显示进度，支持 mp3/flac/wav 输出 |
| 9 | **界面美化（QSS）** |  — 暗色主题，圆角控件，悬停效果，自定义滑块和按钮样式 |



- **多格式音频扫描**（mp3, flac, wav, ogg, wma, m4a, aac）
- **歌词偏移持久化**（按歌曲保存在 library JSON 中）
- **自动识别 .lrc 文件中的 [offset:] 标签**作为初始偏移值
- **GBK 编码回退**：中文 .lrc 文件 UTF-8 解码失败时自动尝试 GBK
- **元数据自动发现**：首次播放时通过 QMediaMetaData 读取 ID3 标签并持久化
- **兼容 [mm:ss] 和 [mm:ss.xx] 两种 .lrc 时间戳格式**



```bash
cd music-player
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./MusicPlayer
```

### MinGW 编译

```bash
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make
```

## 使用说明

1. **扫描歌曲**：点击工具栏"Scan"按钮，选择包含音频文件的文件夹
2. **追加歌曲**：点击"Add"按钮，扫描其他文件夹（不删除已有曲目）
3. **播放歌曲**：双击播放列表中的曲目
4. **搜索**：在搜索框中输入关键字过滤曲目
5. **切换播放模式**：点击底部右侧的模式按钮：列表循环 → 单曲循环 → 随机播放
6. **显示歌词**：将 `.lrc` 歌词文件与音频文件放在同一文件夹，文件名相同即可
7. **微调歌词**：使用 -0.5s / +0.5s 按钮校正对不齐的歌词
8. **翻阅歌词**：在歌词区域拖拽或滚轮翻看，点击居中行的播放按钮跳转到该句
9. **转码**：选中曲目，点击"Convert"，选择输出格式和保存位置

## 架构与设计

### 类的职责划分

| 类 | 职责 |
|----|------|
| `Track` | 数据类：文件路径、标题、歌手、专辑、时长、歌词偏移量。可序列化为 JSON。 |
| `Playlist` | 曲目集合管理：增删、当前项、上下首导航、搜索过滤、将"下一首"决策委托给策略对象。 |
| `Library` | JSON 持久化层：将曲库保存/加载到 `QStandardPaths::AppDataLocation`。处理已删除文件的清理。 |
| `Player` | 外观类，封装 `QMediaPlayer` + `QAudioOutput`：播放/暂停/停止/跳转/音量。其他代码不直接接触 QMediaPlayer。 |
| `LrcParser` | .lrc 文件解析器：UTF-8/GBK 解码、正则提取时间戳、排序行列表、时间→行号查找。 |
| `LyricsWidget` | 自定义 `QWidget`，重写 `paintEvent`：绘制歌词行（带渐变效果），处理鼠标拖拽/滚轮滚动，绘制居中行跳转按钮，通过 `QTimer` 实现松手后自动回到播放位置。 |
| `Transcoder` | 异步 `QProcess` 封装：非阻塞执行 ffmpeg，从 stderr 解析进度百分比，发射开始/进度/完成信号。 |
| `PlayModeStrategy` | **抽象策略（多态）**：纯虚函数 `nextIndex()`、`name()`、`icon()`。 |
| `MainWindow` | UI 编排：创建布局，连接所有信号槽，处理用户操作。 |

### 多态设计：策略模式

`PlayModeStrategy` 抽象基类是本项目的多态核心：

```cpp
class PlayModeStrategy {
public:
    virtual int nextIndex(int current, int count) const = 0;
    virtual QString name() const = 0;
    virtual QString icon() const = 0;
};
```

三种具体实现编码了不同的行为：
- **ListLoopStrategy**：`(current + 1) % count` — 按顺序循环
- **SingleLoopStrategy**：返回 `current` — 重复当前曲目
- **RandomStrategy**：随机选取不同于当前的索引

`Playlist` 类持有全部三个策略对象，并用一个指针指向当前激活的策略。用户点击模式按钮时，`cyclePlayMode()` 切换指针。`nextIndex()` 方法完全委托给策略——无需 if/else 或 switch。这体现了运行时的**策略模式**，满足了课程对多态的要求。

### 组合关系

- `MainWindow` **组合** `Player`、`Playlist`、`Library`、`LrcParser`、`LyricsWidget` — 窗口拥有这些组件及其生命周期。
- `Player` **组合** `QMediaPlayer` 和 `QAudioOutput` — Player 是拥有 Qt 媒体对象的外观类。
- `Playlist` **组合** 三个 `PlayModeStrategy` 对象 — 持有所���策略，同一时刻激活一个。
- `LyricsWidget` **使用** 解析后的 `LrcLine` 数据 — 通过 `setLyrics()` 从 `LrcParser` 接收数据。

### 关键设计决策

1. **Player 作为外观**：没有其他类直接调用 `QMediaPlayer`。这使得播放后端可替换，并将 Qt 多媒体细节封装在单一类中。

2. **LrcParser 与 LyricsWidget 分离**：解析（文件 I/O、编码检测、正则）与渲染（QPainter、鼠标事件、动画）解耦。每个类可以独立测试。

3. **异步转码**：`Transcoder` 通过 `QProcess` 运行 ffmpeg，结果通过信号传递。UI 在转换过程中保持响应。

4. **持久化策略**：`Library` 写入 `QStandardPaths::AppDataLocation`（平台适配路径）。文件已不存在的曲目在加载时静默丢弃（优雅降级）。

### 遇到的最大难点

歌词的拖拽/滚动/跳转交互需要管理三种状态——跟随播放、用户浏览、等待自动回弹——在 `mousePressEvent`、`mouseMoveEvent`、`mouseReleaseEvent`、`wheelEvent` 和一个单次触发的 `QTimer` 之间协调。浏览时，像素级的滚动偏移映射到行索引，居中行显示一个播放按钮，其点击检测需要考虑窗口坐标。每次用户交互（拖拽、滚动或跳转）都会重置自动回弹计时器，实现了主流音乐 App 中"3 秒无操作自动回弹"的效果。

另一个重要的坑是 .lrc 文件的时间戳格式兼容：许多中文 .lrc 文件使用 `[mm:ss]` 格式（无毫秒部分），而代码最初只支持 `[mm:ss.xx]` 格式。通过将正则表达式中的毫秒捕获组改为可选，同时兼容了两种格式。

## 项目结构

```
.
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp                  # 入口
    ├── mainwindow.h / .cpp       # 主界面窗口
    ├── track.h / .cpp            # 曲目数据结构
    ├── playlist.h / .cpp         # 播放列表管理
    ├── library.h / .cpp          # JSON 持久化
    ├── player.h / .cpp           # QMediaPlayer 封装
    ├── lrcparser.h / .cpp        # .lrc 歌词文件解析
    ├── lyricswidget.h / .cpp     # 歌词显示组件
    ├── transcoder.h / .cpp       # FFmpeg 转码
    └── playmodestrategy.h / .cpp # 播放模式策略（多态）
```

## 备注

- 源代码中的注释和标识符使用英文，以避免 MinGW 下的编码问题。
- UI 中显示的中文来自 .lrc 文件或音频元数据，不硬编码在源代码中。
- 已在 Windows 11 + Qt 6.11.1 上测试通过。
