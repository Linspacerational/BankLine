import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import numpy as np
import os # 导入os模块用于路径操作

# 设置中文字体
# 请根据您的操作系统和Python环境确保以下字体可用。
# 在Windows上，'SimHei'通常可用。在Linux或macOS上，可能需要安装字体，
# 或将'SimHei'替换为系统已有的中文字体，如'Arial Unicode MS'或'WenQuanYi Micro Hei'。
try:
    plt.rcParams['font.sans-serif'] = ['SimHei', 'Arial Unicode MS'] # 尝试使用黑体或Unicode字体
    plt.rcParams['axes.unicode_minus'] = False # 解决负号显示为方块的问题
except Exception:
    print("警告: 无法设置中文字体。图表标题和标签可能显示为方块。")
    print("如果出现此问题，请尝试安装'SimHei'字体，或将代码中的字体名称替换为您的系统上可用的中文字体。")

def read_scores(base_file_path):
    """
    从指定路径读取分数数据，并以结构化的字典形式返回。
    
    Args:
        base_file_path (str): 包含 'forbank', 'forvip', 'forcust' 子目录的基础路径，例如 'log/'。

    Returns:
        dict: 一个字典，键为 'forbank', 'forvip', 'forcust' (代表不同的模拟配置)，
              值为对应的列表，每个列表元素为 [num_tellers, score_bank, score_vip, score_cust]。
              如果文件不存在或数据无效，则返回空字典或部分数据。
    """
    all_scores = {}
    
    # 定义预期读取的场景及其对应的文件路径
    scenario_files = {
        'forbank': os.path.join(base_file_path, 'forbank', 'normalized_scores.txt'),
        'forvip': os.path.join(base_file_path, 'forvip', 'normalized_scores.txt'),
        'forcust': os.path.join(base_file_path, 'forcust', 'normalized_scores.txt')
    }

    for scenario_key, file_path in scenario_files.items():
        scores_list = []
        if not os.path.exists(file_path):
            print(f"警告: 文件未找到 '{file_path}'。跳过此场景的数据读取。")
            continue
            
        print(f"正在读取文件: {file_path}")
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                parts = line.strip().split()
                # 预期格式: [num_tellers, score_bank, score_vip, score_cust]
                if len(parts) == 4:
                    try:
                        # 尝试将字符串转换为整数和浮点数
                        scores_list.append([int(parts[0])] + [float(x) for x in parts[1:]])
                    except ValueError:
                        print(f"警告: 无法解析文件 '{file_path}' 中的行: '{line.strip()}'。跳过此行。")
                        continue
                else:
                    print(f"警告: 文件 '{file_path}' 中行格式不正确: '{line.strip()}'。跳过此行。")

        if scores_list:
            all_scores[scenario_key] = scores_list
        else:
            print(f"警告: 文件 '{file_path}' 未包含任何有效数据。")
            
    return all_scores

def plot_scores(all_scores):
    """
    绘制得分对比图表。生成三个子图，每个子图是柱形图，对比一个维度在不同配置下的平均得分。

    Args:
        all_scores (dict): 包含不同模拟配置下得分的字典，
                           结构为 {配置名称: [[num_tellers, score_bank, score_vip, score_cust], ...]}。
    """
    if not all_scores:
        print("没有有效的得分数据可以绘制图表。")
        return

    # 定义三个子图（三个维度）的信息
    # index: 对应得分在每行数据 [num_tellers, score_bank, score_vip, score_cust] 中的索引
    plot_dimensions = {
        'bank_score': {'index': 1, 'title': '“利好银行”得分平均值对比', 'ylabel': '平均得分 (%)'},
        'vip_score':  {'index': 2, 'title': '“利好VIP客户”得分平均值对比', 'ylabel': '平均得分 (%)'},
        'cust_score': {'index': 3, 'title': '“利好普通客户”得分平均值对比', 'ylabel': '平均得分 (%)'}
    }

    # 定义柱形图的X轴标签（即三种模拟配置）
    bar_labels = ['在“利好银行”配置下模拟', '在“利好VIP客户”配置下模拟', '在“利好普通客户”配置下模拟']
    bar_scenario_keys = ['forbank', 'forvip', 'forcust'] # 对应all_scores中的键，决定柱子的顺序

    # 创建一个包含3个子图的图表
    fig, axes = plt.subplots(1, 3, figsize=(24, 8), sharey=True) # sharey=True 确保Y轴范围一致
    fig.suptitle('不同模拟配置下各评估机制的平均得分对比', fontsize=18, y=1.02) # 主标题

    # 确保axes是一个可迭代的数组，即使只有一个子图
    if len(axes.shape) == 1:
        axes = axes.flatten()

    bar_width = 0.25 # 每根柱子的宽度
    # X轴上柱子的位置
    index = np.arange(len(bar_labels))

    # 遍历每个子图维度并绘制柱形图
    for i, (dim_key, dim_info) in enumerate(plot_dimensions.items()):
        ax = axes[i]
        score_idx = dim_info['index']

        # 收集当前维度在三种模拟配置下的平均得分
        average_scores_for_dim = []
        for config_key in bar_scenario_keys:
            if config_key in all_scores and all_scores[config_key]:
                # 提取当前配置下，当前维度（例如“利好银行”）的所有得分
                # 过滤掉任何非数字值（例如，如果数据中出现“无有效数据”）
                valid_scores = [data[score_idx] for data in all_scores[config_key] 
                                if isinstance(data[score_idx], (float, int))]
                
                if valid_scores:
                    # 计算有效得分的平均值
                    average_scores_for_dim.append(np.mean(valid_scores))
                else:
                    # 如果没有有效数据，将平均值设为0或NaN，此处设为0以便于绘图
                    average_scores_for_dim.append(0) 
            else:
                # 如果某个模拟配置的数据完全缺失，平均值设为0
                average_scores_for_dim.append(0) 

        # 绘制柱形图
        bars = ax.bar(index, average_scores_for_dim, bar_width, 
                      color=['skyblue', 'lightcoral', 'lightgreen'])
        
        # 在每根柱子上方添加数值标签
        for bar in bars:
            yval = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2, yval, 
                    f'{yval:.2f}', va='bottom', ha='center', fontsize=10) # 格式化为两位小数

        ax.set_title(dim_info['title'], fontsize=14)
        ax.set_ylabel(dim_info['ylabel'], fontsize=12)
        ax.set_xticks(index)
        # 旋转X轴标签以防重叠，并设置对齐方式
        ax.set_xticklabels(bar_labels, rotation=45, ha='right', fontsize=10) 
        ax.grid(axis='y', linestyle='--', alpha=0.7) # 只显示Y轴网格，更适合柱形图

    plt.tight_layout(rect=[0, 0.1, 1, 0.95]) # 调整布局，为总标题和旋转的X轴标签留出空间
    plt.show()

if __name__ == "__main__":
    # 指定存放数据的根目录
    base_data_path = 'log/' 
    
    # 检查根目录是否存在
    if not os.path.isdir(base_data_path):
        print(f"错误: 数据根目录 '{base_data_path}' 未找到。")
        print("请确保您已运行数据生成脚本，并且数据文件已正确存放在该目录下，")
        print("例如: log/forbank/normalized_scores.txt 等。")
    else:
        scores_data = read_scores(base_data_path)
        
        # 检查是否成功读取到所有预期的场景数据
        expected_scenario_keys = ['forbank', 'forvip', 'forcust']
        if all(key in scores_data and scores_data[key] for key in expected_scenario_keys):
            plot_scores(scores_data)
            print("可视化柱形图已生成。并保存为 'average_scores_comparison_bar_chart.png'。")
        else:
            print("未能读取到所有必要的得分数据。请检查上述警告信息和文件内容。")