# 厂商栈 2D 现场取证（2026-11-16，jmgpu 1.8.0 运行态）

## 文件

- `vendor-2d-ring.txt` / `vendor-2d-ring-1024.txt`
  `vdump resource2 0x213000` 抓到的 2D waitlink 环内容（`0x96C200 = 0x20021300`
  ⇒ CMDADDR=0x21300、MODE=WAITLINK、FROM=MEM）。在 `xcopy` 循环里抓的。
- `vendor-top-2d.txt`：`0x9B0000-0x9B0110` TOP 寄存器 + 2D 全局寄存器快照。
  其中 `0x9B0014 = 0x00010101`、`0x9B0018 = 0x00030101`、`0x9B0048 = 0x0000fff7`、
  `0x9B004C = 0x00010101`（bit0/bit8 也置着），`0x9B0008 = 0x00030303`。

## 提交接口（`libdrm_jmgpu.so.1.0.0`，`drm_jmgpu_j2d_send_cmd` 反汇编）

```
struct { void *cmd; uint32_t size; uint32_t flags; } arg = { cmd, size, 1 };
drmIoctl(fd, 0xc010644b, &arg);
```

内核侧 `jmgpu_garbage.c: j9_handle_j9m_muriculate()` → `j9_handle_insurrecto()`
（`jmgpu_define.c:449`）→ waitlink 引擎的 `j9_nudophobia()`（`jmgpu_accessible.c`）：
检查 size%4==0、size ≤ `J9_HANDLE__HEXAHYDRIC`、取空闲 cmdbuf（`j9_prominences`）、
`j9_handle_j9m_steeliness(cmdbuf->va + sizeof(j9_reopposing), cmds, size, from_user)`
拷贝用户命令、拼 sync 描述符 + wait 尾、改上一槽的 link、踢 `0x4C00`。

## 环里的真实命令流（节选）

```
0x213090: 40001010 01e0022c  40001018 02d10680
0x2130a0: 40001054 ffffffff  8200001c 000200f0
0x2130b0: 01e001ae 02cd0080  00890012 01e0022c
0x2130c0: 02d10680 00890012  00000000 00000000
0x2130d0: 00000000 00000000  00000000 81000000
```

要点：

1. **链头首字是 `0x00000000`**（不是我们用的 `0x80000000`），第二字是
   `byte16_cnt`（例：`0x00000008` → 链长 9×16=144 B）。
2. 寄存器写是 `寄存器 | 值` 交替：`0x40001010 = 0x01e0022c`、`0x40001018 = 0x02d10680`
   —— `0x01e0*16 = 7680`（正好 1920×4 的 pitch），低 16 位是小偏移；
   即这两个"地址"寄存器实际装的是 **(pitch/16)<<16 | 偏移**，不是裸地址。
3. 拷贝 opcode `0x8200001c` 后是 **7 个操作数**：
   `000200f0 | <dst? 01e001ae> | <src? 02cd0080> | <wh 00890012> | <dst 01e0022c> | <src 02d10680> | <wh 00890012>`
   —— 两个"地址三元组"，每组 = (dst, src, w<<16|h)。
4. `0x81000000` 之后紧跟 sync 写 `40006004 00005555` 与 wait/link 尾
   （`89000000 <next_slot>`），说明**END 之后仍会执行**，我们此前"END 截断"的结论
   至少在厂商的链布局下不成立——差别可能在 `byte16_cnt` 是否覆盖后续字。

## 与我们 `tests/j2d-copy/j2dtest.c` 的差异（缺口）

- 我们传的是**裸地址**到操作数/`0x01054`，而厂商把 **(pitch/16)<<16|offset** 放进
  `0x01010`/`0x01018`，并把 (dst,src,wh) 三元组重复两遍；
- 我们的链头用 `0x80000000`，厂商用 `0x00000000`；
- 我们在 `0x01054` 传 dart/desc，厂商直接 `0xffffffff`。

⇒ 下一步：按上面模板重写 `j2dtest` 的命令（先只做一次 fill/copy），在自研栈上验证。
