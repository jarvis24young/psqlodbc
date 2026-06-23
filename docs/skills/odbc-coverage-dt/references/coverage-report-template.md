# Coverage Report Template

Save as `test/docs/<feature>-coverage-report.md`. This is the deliverable that proves the DT actually covers what the analysis doc claimed.

## Required sections

```markdown
# <targetfile>.c <feature> DT 测试覆盖率报告

## 一、执行摘要
- 目标函数: `<targetfunction>()` (`<targetfile>.c:<start>-<end>`)
- 测试套件: `test/src/<feature>-test.c` (N 个用例, TC01–TCNN)
- 执行环境: Ubuntu 24.04 (WSL2) + PostgreSQL 16 + unixODBC 2.3.12
- 编译选项: `-O0 -g -fprofile-arcs -ftest-coverage`
- 总体结论:
  - N 个用例 **全部通过，无 crash / segfault / stack overflow**
  - 函数调用 **M 次**，返回率 X%
  - 行覆盖 **X%** / 分支覆盖 **Y%**
  - <N 条> 分支仍未覆盖，已定位到精确源码行

## 二、构建与执行环境
| 项目 | 值 |
|------|---|
| OS | Ubuntu 24.04 LTS (WSL2) |
| Compiler | gcc <version> |
| PostgreSQL | <version> (端口 5432，本地) |
| unixODBC | <version> |
| 测试 DSN | `<dsn name>` |
| 驱动 | `~/psqlodbc-build/.libs/psqlodbcw.so` |

## 三、测试执行结果
<Table: TC | conn string | SQL return | branch path taken>

## 四、分支覆盖矩阵（gcov -b）

### 4.1 函数级
<gcov one-liner output: calls / returns / blocks / lines / branches>

### 4.2 各分支命中详情
| 分支 ID | 源码行 | 含义 | gcov 命中 | 状态 |
|--------|-------|------|---------|------|
| B0 | <line> | <meaning> | <hits> | ✓/✗ |
...

### 4.3 其他未覆盖分支（非核心）
<defensive branches, OOM paths, compile-time-disabled code — document why each is uncovered>

## 五、关键发现
### 5.1 已覆盖的主要分支
### 5.2 仍需补强的分支
<for each uncovered branch: source line, trigger condition, proposed new TC>

### 5.3 潜在缺陷再评估
<walk through the risk points from the analysis doc; mark which survived empirical testing>

## 六、产物与位置
| 文件 | 路径 |
|------|---------|
| 被测驱动 (.so + gcov 插桩) | `~/psqlodbc-build/.libs/psqlodbcw.so` |
| gcov 数据 | `~/psqlodbc-build/.libs/*_la-<target>.gcda/gcno` |
| 测试可执行 | `~/psqlodbc-build/test/exe/<feature>-test` |
| 覆盖率报告 | `~/psqlodbc-build/<target>.c.gcov` |
| HTML 报告 | `D:\GaussDB\psqlodbc\test\docs\coverage-html\index.html` |

## 七、复现命令
```bash
# reproducible recipe: bootstrap → configure → make → test → gcov
```

## 八、下一步建议
1. 补 N 个 TC 覆盖剩余分支
2. 启用 ASan/UBSan 二次回归
3. 纳入 CI
4. Fuzz 扩展
```

## Quality bar

- **Every Bx in the analysis doc appears in section 4.2** with an explicit hit count and status. Missing entries = incomplete report.
- **Uncovered branches include a proposed TC**. Saying "this branch is uncovered" without saying how to fix it is half a report.
- **Reproduce commands are copy-pasteable**. Someone returning to this report 3 months later should be able to rerun the whole thing without guessing.
- **Compare against analysis-doc risks**. Section 5.3 is where you say "risk R1 was confirmed non-crashing in TC04/TC12" or "risk R3 remains untested, need TC21".

## Reference

See `test/docs/bracket-parse-coverage-report.md` in the psqlodbc repo for a real example.
