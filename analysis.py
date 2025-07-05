import subprocess
import re
import numpy as np

# --- Configuration ---
# simLength numTellers arrivalLow arrivalHigh serviceLow serviceHigh waitHigh
COMMON_PARAMS = {
    'sim_length': 1000, # 模拟总时长
    'arrival_low': 1,   # 客户抵达时间间隔下限
    'arrival_high': 4,  # 客户抵达时间间隔上限
    'service_low': 10,  # 服务时间下限
    'service_high': 30, # 服务时间上限
    'wait_high': 15,    # VIP客户最大容忍等待时间 (固定值)
}
NUM_RUNS_PER_SCENARIO = 100 # 每种出纳员数量和利好场景的运行次数
TELLER_IDLE_ALPHA = 1

# 定义要测试的出纳员数量
num_tellers_values = [2,3,4,5,6,7,8] # 2到10个出纳员

def run_simulation(sim_input):
    """
    Runs the a.exe simulation with the given input string and captures output.
    Args:
        sim_input (str): A string containing space-separated input values.
    Returns:
        dict: A dictionary containing the captured metrics, or None if parsing fails.
    """
    try:
        subprocess.run(['g++', 'a.cpp', '-o', 'a.exe'], check=True) # 编译新的 a.exe 文件
        process = subprocess.run(
            ['./a.exe'],
            input=sim_input.encode('utf-8'), # input 仍然编码为 bytes
            capture_output=True,
            check=True
        )
        subprocess.run(['rm', '-f', 'a.exe'], check=True) # 清理旧的 a.exe 文件
        # 手动将字节输出解码为 GBK 字符串，并忽略无法解码的错误
        output = process.stdout.decode('gbk', errors='ignore') 

        # 使用正则表达式提取各项指标
        avg_vip_wait_match = re.search(r"VIP客户平均等待时间：([\d.]+) 分钟", output)
        avg_teller_idle_match = re.search(r"出纳员平均空闲时间：([\d.]+) 分钟", output)
        avg_cut_in_match = re.search(r"普通用户被插队所平均多出的等待时间：([\d.]+) 分钟", output)
        
        total_customers_match = re.search(r"客户总数：(\d+)", output)
        sim_length_match = re.search(r"模拟时间：(\d+) 分钟", output)
        num_tellers_match = re.search(r"出纳员数量：(\d+)", output)
        
        metrics = {}
        if avg_vip_wait_match:
            metrics['avg_vip_wait_time'] = float(avg_vip_wait_match.group(1))
        if avg_teller_idle_match:
            metrics['avg_teller_idle_time'] = float(avg_teller_idle_match.group(1))
        if avg_cut_in_match:
            metrics['avg_cut_in_time'] = float(avg_cut_in_match.group(1))
        if total_customers_match:
            metrics['total_customers'] = int(total_customers_match.group(1))
        if sim_length_match:
            metrics['sim_length'] = int(sim_length_match.group(1))
        if num_tellers_match:
            metrics['num_tellers'] = int(num_tellers_match.group(1))

        # 检查是否所有关键指标都已解析成功
        if all(k in metrics for k in ['avg_vip_wait_time', 'avg_teller_idle_time', 'avg_cut_in_time', 'total_customers']):
            return metrics
        else:
            #print(f"Warning: Could not parse all expected metrics from output for input: '{sim_input}'. Output may be incomplete or malformed.")
            return None

    except subprocess.CalledProcessError as e:
        print(f"Error running simulation: Command '{e.cmd}' exited with code {e.returncode}")
        # 尝试解码 stderr，并忽略错误
        print(f"Stderr: {e.stderr.decode('gbk', errors='ignore')}") 
        return None
    except FileNotFoundError:
        print("Error: 'a.exe' not found. Make sure it's in the same directory as the script or provide the full path.")
        return None
    except Exception as e:
        print(f"An unexpected error occurred during simulation run: {e}")
        return None

def calculate_score(metrics, weights):
    """
    Calculates the score based on the provided formula and weights.
    score = (float)(cumCustomers * 1000) / (s->totalTellerIdleTime*W_BANK + s->totalVipWaitTime*W_VIP + s->totalCutInTime*W_CUST)
    """
    cum_customers = metrics.get('total_customers', 0)
    total_teller_idle_time = metrics.get('total_teller_idle_time', 0)
    avg_vip_wait_time = metrics.get('avg_vip_wait_time', 0)
    total_cut_in_time = metrics.get('total_cut_in_time', 0)

    denominator = (total_teller_idle_time * weights['W_BANK'] +
                   avg_vip_wait_time * weights['W_VIP'] * TELLER_IDLE_ALPHA +
                   total_cut_in_time * weights['W_CUST'])
    
    # 如果分母为0，则表示无不满，分数为无穷大（完美得分）
    if denominator == 0:
        return float('inf') 

    score = (float)(1000) / denominator
    return score

# --- Define Weight Scenarios ---
# 权重之和为1，方便理解其相对重要性
weights_bank_favored = {
    'W_BANK': 0.7,   # 银行空闲时间权重高
    'W_VIP': 0.15,   # VIP客户等待时间权重低
    'W_CUST': 0.15   # 普通客户被插队时间权重低
}

weights_vip_favored = {
    'W_BANK': 0.15,
    'W_VIP': 0.7,    # VIP客户等待时间权重高
    'W_CUST': 0.15
}

weights_cust_favored = {
    'W_BANK': 0.15,
    'W_VIP': 0.15,
    'W_CUST': 0.7     # 普通客户被插队时间权重高
}

scenarios = {
    "利好银行": weights_bank_favored,
    "利好VIP客户": weights_vip_favored,
    "利好普通客户": weights_cust_favored
}

# --- Run Simulations and Calculate Scores for Each Teller Count and Scenario ---
print(f"--- 评估机制得分 ---")
print(f"固定参数: 模拟时间={COMMON_PARAMS['sim_length']}分钟, 抵达时间={COMMON_PARAMS['arrival_low']}-{COMMON_PARAMS['arrival_high']}分钟, 服务时间={COMMON_PARAMS['service_low']}-{COMMON_PARAMS['service_high']}分钟, VIP最大等待时间={COMMON_PARAMS['wait_high']}分钟")
print(f"每种配置运行 {NUM_RUNS_PER_SCENARIO} 次。")

# 存储所有结果，按出纳员数量分组
all_results = {} 

for num_tellers in num_tellers_values:
    print(f"\n--- 评估出纳员数量: {num_tellers} ---")
    # 构建当前出纳员数量下的模拟输入字符串
    current_sim_input_str = f"{COMMON_PARAMS['sim_length']} {num_tellers} {COMMON_PARAMS['arrival_low']} {COMMON_PARAMS['arrival_high']} {COMMON_PARAMS['service_low']} {COMMON_PARAMS['service_high']} {COMMON_PARAMS['wait_high']}"
    
    # 存储当前出纳员数量下的各个利好场景得分
    all_results[num_tellers] = {}

    for scenario_name, weights in scenarios.items():
        scores = []
        for i in range(NUM_RUNS_PER_SCENARIO):
            metrics = run_simulation(current_sim_input_str)
            if metrics:
                score = calculate_score(metrics, weights)
                scores.append(score)
            else:
                # 仅在获取数据失败时打印警告
                # print(f"Warning: 在出纳员数量为 {num_tellers} 的'{scenario_name}'场景的第 {i+1} 次模拟中未能获取有效数据。")
                pass
                
        if scores:
            average_score = np.mean(scores)
            all_results[num_tellers][scenario_name] = average_score
        else:
            all_results[num_tellers][scenario_name] = "无有效数据"

print("\n--- 最终结果：各出纳员数量下不同利好机制的平均得分 ---")
for num_tellers, results_by_scenario in all_results.items():
    print(f"\n出纳员数量: {num_tellers}")
    for scenario_name, avg_score in results_by_scenario.items():
        if isinstance(avg_score, float):
            print(f"  {scenario_name}: {avg_score:.2f}")
        else:
            print(f"  {scenario_name}: {avg_score}")

print("\n--- 最终结果：各出纳员数量下不同利好机制的得分百分比 (折算后) ---")
# 用于存储最终百分比结果的Python数组
normalized_scores_output = []

for num_tellers in sorted(all_results.keys()): # 确保按出纳员数量排序
    scenario_scores = all_results[num_tellers]
    
    # 提取当前出纳员数量下的三个分数
    score_bank = scenario_scores.get("利好银行")
    score_vip = scenario_scores.get("利好VIP客户")
    score_cust = scenario_scores.get("利好普通客户")

    # 检查所有分数是否有效（浮点数），避免“无有效数据”参与计算
    if isinstance(score_bank, float) and isinstance(score_vip, float) and isinstance(score_cust, float):
        total_sum = score_bank + score_vip + score_cust
        
        if total_sum > 0: # 避免除以零
            pct_bank = (score_bank / total_sum) * 100
            pct_vip = (score_vip / total_sum) * 100
            pct_cust = (score_cust / total_sum) * 100
            
            # 将结果添加到数组中
            normalized_scores_output.append([num_tellers, round(pct_bank, 2), round(pct_vip, 2), round(pct_cust, 2)])
        else:
            # 如果总和为0，表示所有分数都为0，百分比也为0
            normalized_scores_output.append([num_tellers, 0.0, 0.0, 0.0])
    else:
        # 如果有任何一个分数是无效的，则该行数据设为None或者特定的标记
        normalized_scores_output.append([num_tellers, "N/A", "N/A", "N/A"])
        print(f"Warning: 出纳员数量 {num_tellers} 的部分数据无效，无法计算百分比。")

writer = open("log/forcust/normalized_scores.txt", "w", encoding="utf-8")
for row in normalized_scores_output:
    for i in row:
        writer.write(f"{i}\t")
    writer.write("\n")
writer.close()
print("已将百分比结果写入 log/forcust/normalized_scores.txt")

print("\n--- 评估完成 ---")