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
    绘制得分对比图表。生成三个子图，每个子图对比一个维度在不同配置下的得分。

    Args:
        all_scores (dict): 包含不同模拟配置下得分的字典，
                           结构为 {配置名称: [[num_tellers, score_bank, score_vip, score_cust], ...]}。
    """
    if not all_scores:
        print("没有有效的得分数据可以绘制图表。")
        return

    # 尝试从第一个可用的场景中提取出纳员数量列表，确保其已排序且一致
    # 假设所有文件的出纳员数量范围和顺序都是一致的
    teller_counts = []
    for scenario_key in ['forbank', 'forvip', 'forcust']:
        if scenario_key in all_scores and all_scores[scenario_key]:
            teller_counts = sorted(list(set([data[0] for data in all_scores[scenario_key]])))
            break
    
    if not teller_counts:
        print("无法提取出纳员数量数据，无法绘图。")
        return

    # 定义绘图中每条线的标签，表示数据来源于哪种模拟配置
    line_config_labels = {
        'forbank': '在“利好银行”配置下模拟',
        'forvip': '在“利好VIP客户”配置下模拟',
        'forcust': '在“利好普通客户”配置下模拟'
    }

    # 定义三个子图（三个维度）的信息
    # index: 对应得分在每行数据 [num_tellers, score_bank, score_vip, score_cust] 中的索引
    plot_dimensions = {
        'bank_score': {'index': 1, 'title': '“利好银行”得分在不同模拟配置下的变化', 'ylabel': '利好银行得分 (%)'},
        'vip_score':  {'index': 2, 'title': '“利好VIP客户”得分在不同模拟配置下的变化', 'ylabel': '利好VIP客户得分 (%)'},
        'cust_score': {'index': 3, 'title': '“利好普通客户”得分在不同模拟配置下的变化', 'ylabel': '利好普通客户得分 (%)'}
    }

    # 创建一个包含3个子图的图表
    fig, axes = plt.subplots(1, 3, figsize=(24, 7), sharex=True, sharey=True) # sharex/sharey确保X/Y轴范围一致
    fig.suptitle('不同出纳员数量下，各评估机制得分在不同模拟配置下的对比', fontsize=18, y=1.02) # 主标题

    # 遍历每个子图维度并绘制
    for i, (dim_key, dim_info) in enumerate(plot_dimensions.items()):
        ax = axes[i] # 获取当前子图对象
        score_idx = dim_info['index'] # 获取当前维度对应的得分索引

        # 在当前子图中，为每种模拟配置绘制一条线
        for config_key, scores_list in all_scores.items():
            # 提取当前配置下，当前维度（例如“利好银行”）的得分
            # 确保只包含浮点数或整数的有效数据，并与出纳员数量对齐
            current_dim_scores = []
            current_teller_counts_for_plot = []
            
            # 根据teller_counts的顺序收集数据
            for tc in teller_counts:
                found_data = False
                for data_row in scores_list:
                    if data_row[0] == tc:
                        if isinstance(data_row[score_idx], (float, int)):
                            current_dim_scores.append(data_row[score_idx])
                            current_teller_counts_for_plot.append(tc)
                        else:
                            current_dim_scores.append(np.nan) # 插入NaN以创建断线
                            current_teller_counts_for_plot.append(tc)
                        found_data = True
                        break
                if not found_data:
                    current_dim_scores.append(np.nan) # 如果缺少该出纳员数量的数据
                    current_teller_counts_for_plot.append(tc)
            
            ax.plot(current_teller_counts_for_plot, current_dim_scores, 
                    marker='o', linestyle='-', label=line_config_labels.get(config_key, config_key))
        
        ax.set_title(dim_info['title'], fontsize=14)
        ax.set_xlabel('出纳员数量', fontsize=12)
        ax.set_ylabel(dim_info['ylabel'], fontsize=12)
        ax.set_xticks(teller_counts) # 确保X轴刻度与出纳员数量对应
        ax.grid(True, linestyle='--', alpha=0.7) # 添加网格
        ax.legend(title='模拟运行配置', fontsize=10, title_fontsize=12) # 显示图例

    plt.tight_layout(rect=[0, 0.03, 1, 0.95]) # 调整布局，为总标题和底部标签留出空间
    plt.show()
    plt.savefig('scores_comparison.png', dpi=300, bbox_inches='tight') # 保存图表为PNG文件

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
            print("可视化图表已生成。")
        else:
            print("未能读取到所有必要的得分数据。请检查上述警告信息和文件内容。")