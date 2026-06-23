# Analysis Document Template

Save as `test/docs/<feature>-analysis.md`. Goal: make every later decision auditable — anyone reading this doc should be able to reproduce the branch matrix and understand why each TC was written.

## Structure

```markdown
# <targetfile>.c <targetfunction>() 分析与 DT 测试用例设计

## 一、目标代码位置

- **文件**: `<targetfile>.c`
- **函数**: `<targetfunction>()`
- **行号**: <start>–<end>
- **涉及常量**:
  ```c
  // list #define / enum / const values used
  ```

## 二、功能描述

### 2.1 整体作用
<1-2 paragraphs: what the function does, how it's called, destructive modifications, ownership of inputs>

### 2.2 关键变量
| 变量 | 类型 | 含义 |
|------|------|------|
| `var1` | `type` | what it holds and how its lifecycle interacts with the loop |

### 2.3 控制流
<pseudo-code of the decision structure. Annotate each branch with its Bx ID.>

```
case X:
  if (cond1) { B0: ... }
  closep = strchr(...);
  if (cond2 && cond3) { B1: ... }
  for (;;) {
    if (cond4) { B2a: error } else { B2b: recover }
    if (cond5) { B3: escape }
    if (cond6) { B4: valid close }
    B5: error
  }
```

### 2.4 分支清单（DT 覆盖矩阵）
| 分支 ID | 触发条件 | 结果 |
|---------|---------|------|
| B0 | <condition> | <expected outcome> |
| B1 | ... | ... |

## 三、潜在缺陷/风险点
1. <risk 1, with specific line reference>
2. <risk 2>
...

## 四、DT 测试用例设计

测试文件: `test/src/<feature>-test.c`

### 4.1 覆盖矩阵
| TC  | 描述 | 主要分支 | 预期 |
|-----|------|---------|------|
| TC01 | ... | B0 | 不 crash |
| TC02 | ... | B1 → B2 | 不 crash |
...

### 4.2 测试通过判定
- **核心目标**: 无 core dump / 无段错误 / 无 stack overflow
- **次要目标**: <error paths should return expected SQLSTATE>
- 所有 TC 输出 `[No crash - PASS]` 即视为用例通过

### 4.3 断言策略
<how the harness handles SQL errors vs crashes>

## 五、如何在服务器上运行测试
<a concise version of the build + run recipe — refer to WSL setup or Linux/Windows/Sanitizer variants as needed>

## 六、后续扩展建议
- Fuzz 用例 / CI / 多字符集测试 / etc.
```

## Key principles

1. **Branch IDs are anchors.** Once assigned (B0, B1, B2a, etc.), use them consistently across analysis, test comments, and coverage report. Renaming mid-stream breaks the audit trail.
2. **Identify risks BEFORE writing TCs.** The risks section drives the test matrix. If a risk has no TC, you're leaving a known weakness uncovered.
3. **Pseudo-code beats prose.** For decision-heavy functions, a 20-line pseudo-code block says more than 3 paragraphs of description.
4. **Link back to source lines.** Every Bx entry should cite specific source line numbers so future maintainers can jump from the analysis doc to the code without searching.

## Reference

See `test/docs/bracket-parse-analysis.md` for a real example covering `drvconn.c::dconn_get_attributes()`.
