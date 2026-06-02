#ifndef STAGE02_V4L2_CTRLS_HPP_
#define STAGE02_V4L2_CTRLS_HPP_

#include <errno.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "stage02_v4l2_common.hpp"
#include "stage02_v4l2_types.hpp"

namespace stage02_v4l2 {

// ctrl_type_to_string:
//   把 control 的 type 枚举转成人可读文本。
//
// 为什么要做映射：
// 1) 直接打印数字可读性很差；
// 2) 调试时你需要快速知道“这个控件是整数、菜单还是布尔”；
// 3) 后续做自动化配置时可以按类型分流策略。
inline std::string ctrl_type_to_string(uint32_t t) {
    switch (t) {
        case V4L2_CTRL_TYPE_INTEGER:
            return "INTEGER";
        case V4L2_CTRL_TYPE_BOOLEAN:
            return "BOOLEAN";
        case V4L2_CTRL_TYPE_MENU:
            return "MENU";
        case V4L2_CTRL_TYPE_BUTTON:
            return "BUTTON";
        case V4L2_CTRL_TYPE_INTEGER64:
            return "INTEGER64";
        case V4L2_CTRL_TYPE_CTRL_CLASS:
            return "CTRL_CLASS";
        case V4L2_CTRL_TYPE_STRING:
            return "STRING";
        case V4L2_CTRL_TYPE_BITMASK:
            return "BITMASK";
        case V4L2_CTRL_TYPE_INTEGER_MENU:
            return "INTEGER_MENU";
        default:
            return "OTHER";
    }
}

// is_disabled_ctrl:
//   判断控制项是否被驱动标记为 disabled。
//
// 注意：
// - disabled 不等于不存在；
// - 可能是当前场景暂不可调（例如某些模式下曝光相关控件被锁）。
inline bool is_disabled_ctrl(const v4l2_queryctrl& q) {
    return (q.flags & V4L2_CTRL_FLAG_DISABLED) != 0U;
}

// enumerate_controls:
//   枚举控制项，优先使用 NEXT_CTRL 方式，失败时回退 legacy 范围扫描。
inline std::vector<ControlInfo> enumerate_controls(int fd) {
    std::vector<ControlInfo> out;

    // 现代方式：NEXT_CTRL 遍历
    //
    // 核心思想：
    // 1) 把 id 置为 V4L2_CTRL_FLAG_NEXT_CTRL；
    // 2) QUERYCTRL 会返回“下一个可用控件”；
    // 3) 再把 NEXT_CTRL 标志并回去继续迭代。
    //
    // 优势：
    // - 不依赖固定 id 区间；
    // - 能拿到更多扩展控件。
    {
        v4l2_queryctrl q;
        memset(&q, 0, sizeof(q));
        q.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        while (xioctl(fd, VIDIOC_QUERYCTRL, &q) == 0) {
            if (!is_disabled_ctrl(q)) {
                ControlInfo ci;
                ci.id = q.id;
                ci.name = (const char*)q.name;
                ci.type = q.type;
                ci.minimum = q.minimum;
                ci.maximum = q.maximum;
                ci.step = q.step;
                ci.default_value = q.default_value;
                ci.flags = q.flags;
                out.push_back(ci);
            }
            // 继续请求下一个控件。
            q.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
    }

    if (!out.empty()) {
        return out;
    }

    // 兼容回退：遍历 USER 类常用范围。
    //
    // 触发条件：
    // - 某些旧驱动/设备对 NEXT_CTRL 支持不完整。
    //
    // 局限：
    // - 只能覆盖一部分控制项范围，不一定完整。
    for (uint32_t id = V4L2_CID_BASE; id < V4L2_CID_LASTP1; ++id) {
        v4l2_queryctrl q;
        memset(&q, 0, sizeof(q));
        q.id = id;
        if (xioctl(fd, VIDIOC_QUERYCTRL, &q) < 0) continue;
        if (is_disabled_ctrl(q)) continue;
        ControlInfo ci;
        ci.id = q.id;
        ci.name = (const char*)q.name;
        ci.type = q.type;
        ci.minimum = q.minimum;
        ci.maximum = q.maximum;
        ci.step = q.step;
        ci.default_value = q.default_value;
        ci.flags = q.flags;
        out.push_back(ci);
    }
    return out;
}

inline void print_controls_table(const std::vector<ControlInfo>& ctrls) {
    printf("controls (%zu):\n", ctrls.size());
    printf("  %-10s %-28s %-12s %-8s %-8s %-8s %-8s %-10s\n",
           "id", "name", "type", "min", "max", "step", "def", "flags");
    for (size_t i = 0; i < ctrls.size(); ++i) {
        const ControlInfo& c = ctrls[i];
        printf("  0x%08x %-28s %-12s %-8d %-8d %-8d %-8d 0x%08x\n",
               c.id,
               c.name.c_str(),
               ctrl_type_to_string(c.type).c_str(),
               c.minimum,
               c.maximum,
               c.step,
               c.default_value,
               c.flags);
    }
}

// make_ctrl_name_map:
//   把 control 名称标准化后映射到 id。
//
// 规范化规则：
// 1) 大写转小写
// 2) 非字母数字转下划线
//
// 示例：
// - "White Balance Temperature" -> "white_balance_temperature"
//
// 价值：
// - 用户在命令行里不需要输入复杂原始名字；
// - 便于脚本化批量调参。
inline std::map<std::string, uint32_t> make_ctrl_name_map(const std::vector<ControlInfo>& ctrls) {
    std::map<std::string, uint32_t> m;
    for (size_t i = 0; i < ctrls.size(); ++i) {
        std::string k = ctrls[i].name;
        for (size_t j = 0; j < k.size(); ++j) {
            if (k[j] >= 'A' && k[j] <= 'Z') {
                k[j] = (char)(k[j] - 'A' + 'a');
            } else if (!((k[j] >= 'a' && k[j] <= 'z') || (k[j] >= '0' && k[j] <= '9'))) {
                k[j] = '_';
            }
        }
        m[k] = ctrls[i].id;
    }
    return m;
}

inline bool query_and_print_ctrl_value(int fd, uint32_t id, const char* tag) {
    v4l2_control c;
    memset(&c, 0, sizeof(c));
    c.id = id;
    if (xioctl(fd, VIDIOC_G_CTRL, &c) < 0) {
        // 常见失败原因：
        // 1) 控件不存在（EINVAL）
        // 2) 当前状态不允许访问（EBUSY/EACCES）
        // 3) 节点不是期望的 capture 节点
        fprintf(stderr, "VIDIOC_G_CTRL(%s/0x%08x) failed: %s\n", tag, id, strerror(errno));
        return false;
    }
    printf("  %s (0x%08x) = %d\n", tag, id, c.value);
    return true;
}

inline bool set_ctrl_value(int fd, uint32_t id, int val, const char* tag) {
    v4l2_control c;
    memset(&c, 0, sizeof(c));
    c.id = id;
    c.value = val;
    if (xioctl(fd, VIDIOC_S_CTRL, &c) < 0) {
        // 常见失败原因：
        // 1) val 越界（小于 minimum 或大于 maximum）
        // 2) val 不满足 step
        // 3) 该控件只读或当前模式不允许修改
        fprintf(stderr, "VIDIOC_S_CTRL(%s/0x%08x=%d) failed: %s\n", tag, id, val, strerror(errno));
        return false;
    }
    return true;
}

// apply_control_requests:
//   根据 key=val 列表设置控制项，并打印 before/after。
//
// 支持 key：
// 1) 形如 "0x00980900" 的十六进制 id
// 2) 形如 "brightness" 的标准化名称（由枚举表生成）
inline bool apply_control_requests(int fd,
                                   const std::vector<ControlSetRequest>& reqs,
                                   const std::vector<ControlInfo>& ctrls) {
    if (reqs.empty()) return true;
    std::map<std::string, uint32_t> name_map = make_ctrl_name_map(ctrls);
    bool all_ok = true;

    printf("apply controls:\n");
    for (size_t i = 0; i < reqs.size(); ++i) {
        const ControlSetRequest& r = reqs[i];
        uint32_t id = 0;
        bool have_id = false;

        // 分支1：key 按十六进制 id 解析。
        if (r.key.size() > 2 && r.key[0] == '0' && (r.key[1] == 'x' || r.key[1] == 'X')) {
            char* end = NULL;
            unsigned long x = strtoul(r.key.c_str(), &end, 16);
            if (end && *end == '\0') {
                id = (uint32_t)x;
                have_id = true;
            }
        } else {
            // 分支2：key 按归一化名称解析。
            std::map<std::string, uint32_t>::iterator it = name_map.find(r.key);
            if (it != name_map.end()) {
                id = it->second;
                have_id = true;
            }
        }

        if (!have_id) {
            // 这里是“参数错误”，不是驱动错误。
            // 下一步建议：先跑 --list-ctrls 看可用名称。
            fprintf(stderr, "  control key not found: %s\n", r.key.c_str());
            all_ok = false;
            continue;
        }

        // 采取 “before -> set -> after” 三段式打印：
        // 这样你能明确看到“是否真正生效”。
        query_and_print_ctrl_value(fd, id, r.key.c_str());
        bool ok_set = set_ctrl_value(fd, id, r.value, r.key.c_str());
        bool ok_get = query_and_print_ctrl_value(fd, id, r.key.c_str());
        if (!ok_set || !ok_get) all_ok = false;
    }
    return all_ok;
}

}  // namespace stage02_v4l2

#endif  // STAGE02_V4L2_CTRLS_HPP_
