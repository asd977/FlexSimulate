# FlexSimulate 安装与维护手册

本文档说明如何在本地环境构建、部署并维护 FlexSimulate（基于 Qt 的材料/方案可视化与模拟工具）。

## 1. 环境要求

- **操作系统**：Windows 10+ 或主流 Linux 发行版（已验证的构建系统需提供兼容的 C++11 工具链）。
- **Qt**：5.15+（含 `widgets`、`network`、`sql`、`gui-private` 模块）。
- **VTK**：9.2，对应的头文件和静态/动态库已放置在仓库的 `vtk/` 目录，并在 `vtk/vtk.pri` 中声明。
- **构建工具**：`qmake` 与兼容的编译器（MSVC 2019/2022、clang 或 gcc）。

## 2. 获取源码

```bash
git clone <repository-url>
cd FlexSimulate
```

## 3. 构建步骤（命令行）

### 3.1 Linux
1. 确保 Qt 安装后已将 `qmake` 加入 PATH，例如：
   ```bash
   export PATH="/opt/Qt/5.15.2/gcc_64/bin:$PATH"
   ```
2. 在仓库根目录执行：
   ```bash
   qmake FlexSimulate.pro
   make -j$(nproc)
   ```
3. 生成的可执行文件位于构建目录下（默认 `./FlexSimulate`）。

### 3.2 Windows（MSVC）
1. 打开 “x64 Native Tools Command Prompt for VS”。
2. 设置 Qt 环境，例如：
   ```cmd
   set PATH=C:\Qt\5.15.2\msvc2019_64\bin;%PATH%
   ```
3. 在仓库根目录运行：
   ```cmd
   qmake FlexSimulate.pro
   nmake
   ```
4. 生成的 `FlexSimulate.exe` 位于当前构建目录。

### 3.3 使用 Qt Creator
1. 通过 Qt Creator 打开 `FlexSimulate.pro`。
2. 确认 Kit 选择了与 VTK 9.2 兼容的 Qt 版本（MSVC 或 GCC）。
3. 根据需要配置 Debug/Release，然后点击构建与运行。

## 4. 运行与数据

- 应用在 `main.cpp` 中设置了默认字体为 **Microsoft YaHei**，建议操作系统已安装该字体以保持界面一致性。
- 示例数据位于 `sample_data/`，其中材料定义在 `sample_data/materials/`；运行时可将该目录与可执行文件放在同级，确保 JSON 数据可被读取。

## 5. 维护指南

### 5.1 依赖升级
- **Qt 升级**：如需迁移到 Qt 6，请检查 `FlexSimulate.pro` 中的模块声明（`QT += widgets` 等）以及 `gui-private` 依赖是否仍受支持。
- **VTK 升级**：
  - 更新 `vtk/` 目录下的头文件与库，同时在 `vtk/vtk.pri` 中调整 `INCLUDEPATH` 与 `LIBS` 名称（分别有 Debug/Release 列表）。
  - 保持 Debug 与 Release 库名称后缀一致（当前 Debug 使用 `-9.2d`，Release 使用 `-9.2`）。

### 5.2 新增/调整数据模型
- 材料与方案字段的键值映射定义在 `JsonPageBuilder.cpp`（如 `D`, `E`, `YS` 等对应物理字段）。修改映射后，确保配套的 JSON 数据字段与界面标签同步更新。
- 需要批量调整标签或字段时，可使用仓库内的 JSON 示例作为参考（`sample_data/materials/material_detail_*.json`）。

### 5.3 常见问题排查
- **链接失败（VTK 库未找到）**：确认构建器能找到 `vtk/vtk.pri` 中列出的库路径，可通过设置 `LD_LIBRARY_PATH`（Linux）或在 Windows 下将 VTK 动态库放入可执行同级目录。
- **界面字体异常**：为保证布局，安装 “Microsoft YaHei” 或将 `main.cpp` 中的字体设置替换为本地可用字体后重新构建。
- **Qt 私有 API 变动**：项目使用了 `gui-private` 模块，若 Qt 升级导致 API 变化，请参照 Qt 文档调整相关调用。

### 5.4 发布与目录结构
- 将可执行文件与 `sample_data/`、`vtk` 库目录打包分发，保证 `vtk/vtk.pri` 中引用的动态库在运行时可被定位。
- 建议在发布包中附带本手册，以便部署与维护。

## 6. 版本控制建议
- 采用 Git 分支开发流程，发布前在 Release/Debug 两种配置下完成编译自测。
- 关键依赖版本（Qt、VTK）变更时，在提交信息中注明版本号，并更新本手册对应章节。

