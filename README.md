# 音乐播放器 — Qt 桌面音乐播放器

仿网易云音乐的桌面音乐播放器，基于 Qt6/C++ 构建。

## 已实现的功能

| # | 功能 | 状态 |
|---|------|------|
| 1 | **扫描歌曲 + 持久化播放列表** |  — 递归扫描文件夹（支持 .mp3/.flac/.wav/.ogg/.wma/.m4a/.aac），通过 `Library` 类以 JSON 持久化 |
| 2 | **基本播放控制** |  — 播放/暂停/停止/上一首/下一首，音量滑块，可拖拽进度条，时间显示（mm:ss / mm:ss） |
| 3 | **歌曲信息显示** |  — 通过 `QMediaMetaData` 读取标题/歌手/专辑信息，无标签时退回文件名 |
| 4 | **歌词同步 + 时间微调** |  — 自动加载同名 .lrc 文件，解析时间戳，高亮当前行，+/- 0.5s 偏移调整（按歌曲持久化） |
| 5 | **歌词拖动滚动 + 跳转** |  — 鼠标拖拽/滚轮翻阅歌词，居中行显示播放跳转按钮，松手 3 秒无操作自动回到当前播放句 |
| 6 | **播放模式** |  — 列表循环 / 单曲循环 / 随机播放，通过模式按钮切换，工具栏显示当前模式 |
| 7 | **搜索/过滤** |  — 输入关键字实时按标题/歌手/专辑过滤 |
| 8 | **批量转码（异步）** |  — 基于 QProcess 调用 ffmpeg，多文件队列转换，可选码率/采样率/输出格式，显示单文件和批量进度 |
| 9 | **歌单管理** |  — 创建/删除/切换歌单，右键添加入歌单，自动生成按歌手/专辑/标签的歌单，歌单固定置顶 |
| 10 | **音频波形可视化** |  — 解码音频文件绘制波形图，随播放进度实时移动播放头 |
| 11 | **卡拉 OK 模式** |  — 逐字高亮渲染歌词，跟随播放进度着色 |


- **多格式音频扫描**（mp3, flac, wav, ogg, wma, m4a, aac）
- **歌词偏移持久化**（按歌曲保存在 library JSON 中）
- **自动识别 .lrc 文件中的 [offset:] 标签**作为初始偏移值
- **GBK 编码回退**：中文 .lrc 文件 UTF-8 解码失败时自动尝试 GBK
- **元数据自动发现**：首次播放时通过 QMediaMetaData 读取 ID3 标签并持久化
- **兼容 [mm:ss] 和 [mm:ss.xx] 两种 .lrc 时间戳格式**

---



---

## 详细使用说明

### 1. 扫描歌曲

点击工具栏 **📁 Scan** 按钮，选择一个包含音频文件的文件夹。程序将递归扫描所有子文件夹，支持的格式包括：`.mp3`、`.flac`、`.wav`、`.ogg`、`.wma`、`.m4a`、`.aac`。

扫描完成后，所有曲目显示在左侧播放列表中，曲库自动以 JSON 格式持久化保存，下次启动时自动加载。

### 2. 追加歌曲

点击工具栏 **➕ Add** 按钮，选择另一个文件夹。新增的曲目将追加到现有曲库，不会删除已扫描的曲目。重复文件（相同路径）会自动跳过。

### 3. 播放歌曲

- **双击**播放列表中的任意曲目即可开始播放
- 底部控制栏提供：⏮ 上一首 / ▶ 播放 / ⏸ 暂停 / ⏹ 停止 / ⏭ 下一首
- 进度条可拖拽跳转到任意位置，时间标签显示"当前时间 / 总时长"
- 音量滑块控制输出音量

### 4. 搜索

在工具栏右侧搜索框中输入关键字，播放列表将实时按**标题、歌手、专辑**过滤显示。清空搜索框恢复完整列表。

### 5. 切换播放模式

点击底部控制栏右侧的模式按钮，在三种播放模式间循环切换：

| 图标 | 模式 | 行为 |
|------|------|------|
| 🔄 | 列表循环 | 顺序播放，播完最后一首回到第一首 |
| 🔂 | 单曲循环 | 重复播放当前曲目 |
| 🔀 | 随机播放 | 随机选择下一首（不会连续两次选同一首） |

工具栏最右侧显示当前模式名称。

### 6. 歌单管理

#### 创建歌单
点击播放列表上方的 **+** 按钮，输入歌单名称，在弹出的对话框中勾选要加入的曲目。支持"全选"/"取消全选"快捷操作。

#### 切换歌单
通过播放列表上方的下拉框选择"All Tracks"或已创建的歌单，播放列表内容随之切换。

#### 快速添加到歌单
正在播放时，点击歌词控制栏的 **+** 按钮，选择目标歌单即可将当前曲目添加进去。

#### 右键菜单
- **播放列表中的曲目**：右键 → "Edit Tags..." 编辑标签 / "Add to Playlist" 加入指定歌单
- **下拉框中的歌单名**：右键 → ▶ Play 播放该歌单 / 📌 Pin to Top 置顶 / 🗑 Delete 删除

#### 自动生成歌单
点击 **Auto** 按钮，可选择按"歌手"/"专辑"/"标签"自动生成歌单，程序根据曲目的元数据自动分组创建。

#### 歌单固定
右键歌单名选择 Pin，该歌单将在下拉列表中置顶显示。

### 7. 歌词显示

#### 自动加载
将 `.lrc` 歌词文件与音频文件放在同一文件夹，保持**文件名相同**（例如 `song.mp3` 对应 `song.lrc`）。播放时自动加载并显示。

程序支持：
- UTF-8 和 GBK 编码的 .lrc 文件
- `[mm:ss.xx]` 和 `[mm:ss]` 两种时间戳格式
- `[offset:±ms]` 标签自动应用初始偏移

#### 同步显示
歌词区域实时高亮当前播放句，并自动滚动居中。

#### 微调偏移
如果歌词与音频不同步，使用 **-0.5s** / **+0.5s** 按钮调整偏移量。偏移值按歌曲持久化保存，下次播放该歌曲时自动应用。

### 8. 浏览歌词

- **拖拽**：在歌词区域按住鼠标上下拖动翻阅
- **滚轮**：鼠标滚轮滚动歌词
- **跳转**：手动翻阅时，居中行左侧显示 ▶ 播放按钮，点击跳转到该句
- **自动回弹**：松手后 3 秒无操作，歌词自动滚动回当前播放位置

### 9. 卡拉 OK 模式

点击工具栏 **🎤 Karaoke** 按钮切换卡拉 OK 模式。开启后，当前歌词行将**逐字渲染**——已播放的部分着色高亮，未播放的部分保持暗色，实现类似 KTV 字幕的渐进着色效果。

### 10. 音频波形可视化

右侧面板中嵌入频谱可视化组件（`SpectrumWidget`），通过 `QAudioDecoder` 解码音频文件，计算每段的能量级别并绘制波形柱状图。播放时，播放头随进度实时移动。

### 11. 主题切换

点击工具栏 **🌙 Dark** / **☀️ Light** 按钮切换暗色/亮色主题。主题偏好自动持久化，下次启动时恢复。

### 12. 批量转码（Convert）

这是本版本的重点功能，支持批量转换音频格式、自定义码率和采样率。

#### 基本流程

1. **选中曲目**：在播放列表中使用 Ctrl+点击 或 Shift+点击 选中一个或多个曲目（支持多选）
2. **打开转换**：点击工具栏 **🔄 Convert** 按钮
3. **配置参数**：在弹出的对话框中设置：
   - **输出格式**：MP3 / FLAC / WAV
   - **码率（Bitrate）**：
     - Default (VBR q=2)：使用 ffmpeg 默认可变码率，品质系数 2
     - 128 kbps：适合语音/播客，文件较小
     - 192 kbps：适合一般音乐，品质与大小平衡
     - 256 kbps：高品质音乐
     - 320 kbps：最高品质 MP3
   - **采样率（Sample Rate）**：
     - Keep original：保持原始采样率
     - 44100 Hz：CD 音质标准
     - 48000 Hz：视频/专业音频标准
   - **输出目录**：默认为第一个选中曲目所在的文件夹，可点击 Browse 更改
4. **开始转换**：点击 **Start Convert**，程序开始逐文件转换

#### 进度显示

转换过程中，状态栏实时显示：
- 当前文件进度：`[1/5] Converting: song.mp3... 45%`
- 完成提示：`[1/5] Done: song.mp3`
- 批量完成：`Batch conversion complete — 5 file(s)`

#### 批量队列

- 文件按选中顺序依次转换（一个接一个，后台异步执行）
- 单个文件失败不会中断队列，自动跳过继续处理下一个
- 输出文件名 = 原始文件名 + 所选格式后缀（同名自动加 `_converted` 避免覆盖源文件）
- 转换过程中界面保持响应，不会卡顿

#### 注意事项

- 需要安装 FFmpeg（`ffmpeg.exe` 在 PATH 或可执行文件同目录下）
- FLAC 为无损格式，码率选项设为最高压缩级别
- WAV 为无压缩格式，码率选项不生效
- 转换速度取决于音频文件时长和 CPU 性能

---

## 代码架构

### 类的职责划分

| 类 | 文件 | 职责 |
|----|------|------|
| `Track` | `track.h/cpp` | 数据类：文件路径、标题、歌手、专辑、时长、歌词偏移量、标签列表。可序列化为 JSON。 |
| `Playlist` | `playlist.h/cpp` | 曲目集合管理：增删、当前项、上下首导航、搜索过滤、将"下一首"决策委托给策略对象。 |
| `PlaylistManager` | `playlistmanager.h/cpp` | 多歌单管理：用户歌单和自动生成歌单的 CRUD、固定置顶、歌单间曲目增删、JSON 持久化。 |
| `PlaylistEntry` | `playlistmanager.h` | 数据结构：单个歌单的名称、类型（user/artist/album/tag）、曲目路径列表、固定标记。 |
| `Library` | `library.h/cpp` | JSON 持久化层：将完整曲库（所有 Track 元数据）保存/加载到 `QStandardPaths::AppDataLocation`。自动清理已删除文件的引用。 |
| `Player` | `player.h/cpp` | **外观类**，封装 `QMediaPlayer` + `QAudioOutput`：播放/暂停/停止/跳转/音量。其他代码不直接接触 Qt 多媒体 API。 |
| `LrcParser` | `lrcparser.h/cpp` | .lrc 文件解析器：UTF-8/GBK 编码检测、正则提取时间戳（兼容 [mm:ss] 和 [mm:ss.xx]）、排序行列表、时间→行号查找。 |
| `LyricsWidget` | `lyricswidget.h/cpp` | 自定义 `QWidget`，重写 `paintEvent`：绘制歌词行（带渐变效果），处理鼠标拖拽/滚轮滚动，绘制居中行跳转按钮，通过 `QTimer` 实现松手后自动回弹。 |
| `LyricsRenderer` | `lyricsrenderer.h/cpp` | **抽象策略（多态）**：歌词渲染接口，定义 `paintLine()` 纯虚函数。 |
| `SimpleLyricsRenderer` | `lyricsrenderer.cpp` | 普通歌词渲染器：整行统一着色。 |
| `KaraokeLyricsRenderer` | `lyricsrenderer.cpp` | 卡拉 OK 渲染器：逐字符着色，按播放进度从左到右依次高亮。 |
| `SpectrumWidget` | `spectrumwidget.h/cpp` | 音频波形可视化：通过 `QAudioDecoder` 解码音频文件，计算分段能量级别，绘制波形柱状图，随播放进度实时移动播放头。 |
| `Transcoder` | `transcoder.h/cpp` | 异步 `QProcess` 封装：非阻塞执行 ffmpeg，支持自定义码率和采样率，从 stderr 解析实时进度百分比，通过 Qt 信号发射开始/进度/完成事件。 |
| `PlayModeStrategy` | `playmodestrategy.h/cpp` | **抽象策略（多态）**：纯虚函数 `nextIndex()`、`name()`、`icon()`。 |
| `ListLoopStrategy` | `playmodestrategy.cpp` | 列表循环：`(current + 1) % count`。 |
| `SingleLoopStrategy` | `playmodestrategy.cpp` | 单曲循环：返回 `current`。 |
| `RandomStrategy` | `playmodestrategy.cpp` | 随机播放，不连续重复同一曲目。 |
| `MainWindow` | `mainwindow.h/cpp` | UI 编排中心：创建布局和控件，连接所有信号槽，管理批量转码队列，处理所有用户操作。 |

### 多态设计：策略模式

本项目采用两种策略模式，体现不同的多态应用场景：

#### 1. 播放模式策略（PlayModeStrategy）

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

`Playlist` 类持有全部三个策略对象，并用一个指针指向当前激活的策略。用户点击模式按钮时，`cyclePlayMode()` 切换指针。`nextIndex()` 完全委托给策略——无需 if/else 或 switch。

#### 2. 歌词渲染策略（LyricsRenderer）

```cpp
class LyricsRenderer {
public:
    virtual void paintLine(QPainter *p, const QRect &rect,
                           const QString &text, bool isActive,
                           float charProgress) const = 0;
};
```

两种实现：
- **SimpleLyricsRenderer**：普通模式，整行统一着色（高亮/非高亮）
- **KaraokeLyricsRenderer**：卡拉 OK 模式，逐字符计算着色区域，按 `charProgress`（0.0–1.0）从左到右渐进渲染

`LyricsWidget` 通过 `setRenderer()` 切换渲染策略，用户点击 Karaoke 按钮时动态切换。

### 组合关系

- `MainWindow` **组合** `Player`、`Playlist`、`PlaylistManager`、`Library`、`LrcParser`、`LyricsWidget`、`SpectrumWidget`、`Transcoder` — 窗口拥有这些组件并管理其生命周期。
- `Player` **组合** `QMediaPlayer` 和 `QAudioOutput` — Player 是拥有 Qt 媒体对象的外观类。
- `Playlist` **组合** 三个 `PlayModeStrategy` 对象 — 持有所有策略，同一时刻激活一个。
- `LyricsWidget` **组合** `LyricsRenderer` 指针 — 持有普通和卡拉 OK 两种渲染器，运行时切换。
- `LyricsWidget` **使用** 解析后的 `LrcLine` 数据 — 通过 `setLyrics()` 从 `LrcParser` 接收数据。
- `PlaylistManager` **管理** 多个 `PlaylistEntry` — 歌单数据和曲目路径的集合。

### 转码架构

#### 单文件可配置转码

`Transcoder` 类封装对 `ffmpeg.exe` 的异步调用：

```
用户 → MainWindow → Transcoder::start(file, format, bitrate, sampleRate)
                         ↓
                    QProcess → ffmpeg.exe (异步, 非阻塞)
                         ↓
                    stderr 解析 → progressChanged(percent)
                         ↓
                    结束 → finished(success, outputFile)
```

关键参数传递：
- **bitrateKbps** = 0：使用 ffmpeg 默认 VBR 品质 (`-q:a 2`)
- **bitrateKbps** > 0：使用固定码率 (`-b:a <N>k`)
- **sampleRateHz** = 0：保持原始采样率
- **sampleRateHz** > 0：重采样 (`-ar <N>`)

#### 批量转码队列

`MainWindow` 管理批量转码队列：

```
用户选中 N 个文件 → 配置对话框 → 确定
        ↓
    m_batchQueue = [file1, file2, ..., fileN]
    m_batchIndex = 0, m_batchTotal = N
        ↓
    processNextBatchItem()
        ↓
    Transcoder::start(fileN)
        ↓ (异步转换... progressChanged 更新状态栏)
    finished 信号
        ↓
    QTimer::singleShot(500ms) → processNextBatchItem()
        ↓
    ... 队列为空 → 批量完成
```

设计要点：
- 队列每次只处理一个文件（串行转换），通过 `QTimer::singleShot` 延迟 500ms 启动下一个，给 UI 时间刷新
- 单个文件失败时跳过，继续处理剩余文件
- 状态栏始终显示 `[当前/总数]` 和当前文件实时百分比
- 输出文件自动避免覆盖源文件（同名加 `_converted` 后缀）

### 关键设计决策

1. **Player 作为外观**：没有其他类直接调用 `QMediaPlayer`。这使得播放后端可替换，并将 Qt 多媒体细节封装在单一类中。

2. **LrcParser 与 LyricsWidget 分离**：解析（文件 I/O、编码检测、正则）与渲染（QPainter、鼠标事件、动画）解耦。每个类可以独立测试。

3. **双重策略模式**：播放模式（PlayModeStrategy）和歌词渲染（LyricsRenderer）都采用策略模式，展示了多态在不同领域——算法选择和视觉渲染——中的灵活应用。

4. **异步转码与批量队列**：`Transcoder` 通过 `QProcess` 运行 ffmpeg，所有操作通过信号通知。`MainWindow` 管理文件队列和批量进度，UI 在转换全程保持响应。

5. **持久化策略**：`Library` 写入 `QStandardPaths::AppDataLocation`（平台适配路径）。文件已不存在的曲目在加载时静默丢弃（优雅降级）。歌词偏移量随曲元数据一起保存。

6. **歌单与曲库分离**：`Library` 保存完整的曲库元数据（Track 列表），`PlaylistManager` 保存歌单定义（名称、类型、曲目路径引用列表）。歌单只存储路径引用，不复制元数据，确保数据一致性。

### 遇到的最大难点

1. **歌词拖拽/滚动/跳转交互**：需要管理三种状态——跟随播放、用户浏览、等待自动回弹——在 `mousePressEvent`、`mouseMoveEvent`、`mouseReleaseEvent`、`wheelEvent` 和一个单次触发的 `QTimer` 之间协调。浏览时，像素级的滚动偏移映射到行索引，居中行显示一个播放按钮，其点击检测需要考虑窗口坐标。每次用户交互都会重置自动回弹计时器，实现了主流音乐 App 中"3 秒无操作自动回弹"的效果。

2. **.lrc 文件编码与格式兼容**：许多中文 .lrc 文件使用 GBK 编码（而非 UTF-8），且时间戳格式为 `[mm:ss]`（无毫秒部分）。解决方案：UTF-8 解码失败时自动回退 GBK，正则表达式毫秒捕获组设为可选。同时还支持 `[offset:±ms]` 标签的自动识别。

3. **批量转码队列管理**：转码是异步的（`QProcess`），需要在单个文件完成后自动启动下一个。使用"槽函数处理完成信号 → QTimer 延迟 → 递归调用 `processNextBatchItem()`"的方案，既保证了顺序执行，又给了 UI 时间刷新状态显示，同时避免了深层递归栈溢出的风险（每次调用都是新的槽函数调用，栈帧独立）。

4. **卡拉 OK 逐字渲染**：需要将歌词文本按字符拆分，根据播放进度（`charProgress` 0.0–1.0）计算每个字符的着色边界。`QPainter` 绘制时，已播放字符用一种颜色绘制，未播放字符用另一种，边界字符部分着色（通过 `QPainterPath` clip 实现）。这需要精确的字符宽度计算（`QFontMetrics`），并且要处理中英文混排的宽度差异。

## 项目结构

```
.
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp                  # 入口，创建 QApplication 和 MainWindow
    ├── mainwindow.h / .cpp       # 主界面窗口：UI 编排、信号槽连接、批量转码队列
    ├── track.h / .cpp            # 曲目数据结构（路径、标题、歌手、专辑、时长、标签、偏移）
    ├── playlist.h / .cpp         # 播放列表：曲目集合管理、搜索过滤、播放模式委托
    ├── playlistmanager.h / .cpp  # 歌单管理器：多歌单 CRUD、自动生成、置顶、持久化
    ├── library.h / .cpp          # 曲库 JSON 持久化（QStandardPaths）
    ├── player.h / .cpp           # QMediaPlayer 外观封装
    ├── lrcparser.h / .cpp        # .lrc 歌词文件解析（UTF-8/GBK、正则、时间戳）
    ├── lyricswidget.h / .cpp     # 歌词显示组件（QPainter 绘制、拖拽/滚动/跳转）
    ├── lyricsrenderer.h / .cpp   # 歌词渲染策略（普通 + 卡拉 OK 逐字着色）
    ├── spectrumwidget.h / .cpp   # 音频波形可视化（QAudioDecoder 解码 + 波形绘制）
    ├── transcoder.h / .cpp       # FFmpeg 异步转码封装（QProcess、进度解析）
    └── playmodestrategy.h / .cpp # 播放模式策略（列表循环 / 单曲循环 / 随机）
```

## 备注

- 源代码中的注释和标识符使用英文，以避免 MinGW 下的编码问题。
- UI 中显示的中文来自 .lrc 文件或音频元数据，不硬编码在源代码中。
- 已在 Windows 11 + Qt 6.11.1 + MinGW 上测试通过。
- FFmpeg 转码功能需要 `ffmpeg.exe` 可用，程序启动时自动检测（检测不通过不影响其他功能使用）。

- 这个大作业也算完成了，应该也没人看捏。