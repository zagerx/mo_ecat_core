# mo_ecat_core

EtherCAT 主站核心库，零 Qt、无外部可执行文件依赖。

## 目录

- `include/mo_ecat/`：公共头文件
- `src/`：核心库实现
- `third_party/SOEM`：SOEM 子模块
- `examples/cli/`：命令行示例
- `docs/`：设计文档与笔记

## 快速构建

核心库不单独生成可执行文件，通过 CLI 示例一起构建，产物统一放在 `examples/cli/build/` 下：

```bash
cd examples/cli
cmake -B build .
cmake --build build -j$(nproc)
```

生成的文件：

```text
examples/cli/build/ecat_cli                # CLI 可执行文件
examples/cli/build/core/libmo_ecat_core.a  # 核心库静态库
```

## 运行示例

```bash
sudo ./build/ecat_cli eth0
```

> 打开原始套接字需要 root 权限。

详见 [docs/others/使用文档.md](docs/others/使用文档.md)。
