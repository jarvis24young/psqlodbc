# Agent Skills

该目录用于保存可复用的 Agent Skill 源码，便于在 GaussDB 驱动团队内共享、评审和二次演进。

## 当前 Skill

- `odbc-coverage-dt/`：面向 ODBC 驱动覆盖率提升和鲁棒性加固的 Design Test 流水线。它引导 Agent 完成源码分析、分支映射、测试编写、gcov/lcov 度量和覆盖率报告生成。

## 本地安装

在 Windows 上，将 Skill 目录复制到 Codex skills 目录：

```powershell
Copy-Item -Recurse -Force docs\skills\odbc-coverage-dt $env:USERPROFILE\.codex\skills\odbc-coverage-dt
```

安装后，可以让 Agent 使用 `$odbc-coverage-dt` 处理 ODBC 覆盖率提升、DT 测试设计或 crash-hardening 任务。
