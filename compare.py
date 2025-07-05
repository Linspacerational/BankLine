import matplotlib.pyplot as plt
import os
from matplotlib import font_manager
# 设置字体以支持中文
font_prop = font_manager.FontProperties(fname='C:/Windows/Fonts/simhei.ttf')
# 设置全局字体
plt.rcParams['font.family'] = font_prop.get_name()

def read_normalized_scores(file_path):
    data = []
    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) >= 4 and all(p not in ("N/A", "") for p in parts[1:4]):
                try:
                    num_tellers = int(parts[0])
                    pct_bank = float(parts[1])
                    pct_vip = float(parts[2])
                    pct_cust = float(parts[3])
                    data.append([num_tellers, pct_bank, pct_vip, pct_cust])
                except ValueError:
                    continue
    return sorted(data, key=lambda x: x[0])

# 读取三个文件
base_dir = "log"
files = {
    "forbank": os.path.join(base_dir, "forbank", "normalized_scores.txt"),
    "forvip": os.path.join(base_dir, "forvip", "normalized_scores.txt"),
    "forcust": os.path.join(base_dir, "forcust", "normalized_scores.txt"),
}

data = {k: read_normalized_scores(v) for k, v in files.items()}

# 提取x轴（出纳员数量）和y轴（百分比）
def extract_lines(data, idx):
    x = [row[0] for row in data]
    y = [row[idx] for row in data]
    return x, y

# 画图函数
def plot_score(idx, title, ylabel, savefile):
    plt.figure(figsize=(8,5))
    for key, label, color in zip(
        ["forbank", "forvip", "forcust"],
        ["利好银行", "利好VIP", "利好普通客户"],
        ["#1f77b4", "#d62728", "#2ca02c"]
    ):
        x, y = extract_lines(data[key], idx)
        plt.plot(x, y, marker='o', label=label)
    plt.xlabel("出纳员数量")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.5)
    plt.tight_layout()
    plt.savefig(savefile)
    plt.show()

# 画三张图
titles = [
    "利好银行机制下各机制得分百分比",
    "利好VIP机制下各机制得分百分比",
    "利好普通客户机制下各机制得分百分比"
]
ylabels = [
    "银行机制得分百分比",
    "VIP机制得分百分比",
    "普通客户机制得分百分比"
]
score_indices = [1, 2, 3]

fig, axes = plt.subplots(3, 1, figsize=(10, 15))

for ax, idx, title, ylabel in zip(axes, score_indices, titles, ylabels):
    for key, label, color in zip(
        ["forbank", "forvip", "forcust"],
        ["利好银行", "利好VIP", "利好普通客户"],
        ["#1f77b4", "#d62728", "#2ca02c"]
    ):
        x, y = extract_lines(data[key], idx)
        ax.plot(x, y, marker='o', label=label)
    ax.set_xlabel("出纳员数量")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle='--', alpha=0.5)

plt.tight_layout()
plt.savefig("score_compare.png")
plt.show()