# Stage01.5 执行清单（2天强化版）

> 目标：不新增复杂代码，先把 Stage01 的核心能力“做实”。
> 节点固定：`/dev/video10`（你当前机器已验证可跑）。

## 0. 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation
./build.sh stage01_v4l2_capture_main
```

## 0.5 推荐：先跑一键版拿全量结果

```bash
./stage01_5_run_all.sh --dev=/dev/video10 --build
```

跑完后直接看 `summary.md` 和 `summary.csv`，再回头按下面 A~E 手工跑 1~2 个 case 做“代码-日志-指标”的对照学习。

## 1. 基线实验（先拿参考值）

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/base.yuyv /tmp/base.ppm 120 --no-save --log-every=20
```

记录：
1. fps(actual dq_ok / duration)
2. select timeout
3. dqbuf fail/eagain
4. bytesused min/max/avg
5. dq host interval avg

## 2. 队列深度对比（req-bufs）

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/b2.yuyv /tmp/b2.ppm 120 --no-save --req-bufs=2 --log-every=20
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/b4.yuyv /tmp/b4.ppm 120 --no-save --req-bufs=4 --log-every=20
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/b8.yuyv /tmp/b8.ppm 120 --no-save --req-bufs=8 --log-every=20
```

你要回答：
1. `req-bufs` 变大后 fps 是否稳定提升？
2. timeout / dq_fail 是否变化？
3. 为什么不是 buffer 越多越好？

## 3. 格式对比（同分辨率）

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/f_yuyv.yuyv /tmp/f_yuyv.ppm 120 --pixfmt=YUYV --no-save --log-every=20
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/f_mjpg.yuyv /tmp/f_mjpg.ppm 120 --pixfmt=MJPG --no-save --log-every=20
```

你要回答：
1. `request -> active(S_FMT) -> G_FMT` 是否一致？
2. 不同 pixfmt 下 `bytesused` 分布差异是什么？
3. 哪个格式更适合“稳定实时预览”？为什么？

## 4. 分辨率对比（同格式）

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/r_640.yuyv /tmp/r_640.ppm 120 --pixfmt=YUYV --no-save --log-every=20
./bin/stage01_v4l2_capture_main /dev/video10 1280 720 /tmp/r_720.yuyv /tmp/r_720.ppm 120 --pixfmt=YUYV --no-save --log-every=20
```

你要回答：
1. 分辨率变大后 fps / interval / timeout 怎么变？
2. 是否出现驱动自动回退分辨率？

## 5. 队列断环故障注入（skip-requeue）

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 /tmp/skip.yuyv /tmp/skip.ppm 80 --inject=skip-requeue --inject-frame=20 --no-save --log-every=20
```

你要回答：
1. 为什么会 timeout？
2. `requeue_skipped` 与 `dq_ok` 的关系是什么？
3. 这说明了哪个不变量？

## 6. 产出

请把结果填到：
- `stage01_5_conclusion_template.md`

完成标准：
1. 所有实验都有命令与结果
2. 有“结论句”，不是只贴日志
3. 能解释至少 3 个因果关系
