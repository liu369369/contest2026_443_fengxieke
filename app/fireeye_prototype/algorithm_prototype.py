#!/usr/bin/env python3
"""
火眼项目 - PC端算法原型
用于验证滤波、阈值判定、状态机等核心算法

使用方法:
    python3 algorithm_prototype.py

功能:
    1. 数据滤波（滑动平均 + 一阶低通）
    2. 阈值判定（单阈值 + 趋势 + 防抖）
    3. 状态机（正常 → 预警 → 报警 → 断电）
    4. 可视化展示
"""

import numpy as np
import matplotlib.pyplot as plt
from collections import deque
from enum import Enum
from dataclasses import dataclass
from typing import List, Tuple
import time

# ============================================================
# 配置参数
# ============================================================

class Config:
    """系统配置参数"""
    # 采样参数
    SAMPLE_RATE = 10  # Hz
    SAMPLE_PERIOD = 1.0 / SAMPLE_RATE
    DURATION = 30  # 秒
    
    # 滤波参数
    MOVING_AVG_WINDOW = 10  # 滑动平均窗口大小
    LOW_PASS_ALPHA = 0.3    # 低通滤波系数 (0-1, 越小越平滑)
    
    # 阈值参数 (电流, 单位: A)
    CURRENT_WARNING_THRESHOLD = 5.0   # 预警阈值
    CURRENT_ALARM_THRESHOLD = 8.0     # 报警阈值
    
    # 阈值参数 (温度, 单位: °C)
    TEMP_WARNING_THRESHOLD = 45.0     # 预警阈值
    TEMP_ALARM_THRESHOLD = 60.0       # 报警阈值
    
    # 防抖参数
    CONSECUTIVE_COUNT = 3  # 连续超限次数才触发
    
    # 趋势参数
    SLOPE_THRESHOLD = 0.5  # 斜率阈值 (A/s 或 °C/s)


# ============================================================
# 数据滤波模块
# ============================================================

class MovingAverageFilter:
    """滑动平均滤波器"""
    
    def __init__(self, window_size: int):
        self.window_size = window_size
        self.buffer = deque(maxlen=window_size)
        self.sum = 0.0
    
    def process(self, value: float) -> float:
        """处理一个新数据点"""
        if len(self.buffer) == self.window_size:
            self.sum -= self.buffer[0]
        
        self.buffer.append(value)
        self.sum += value
        
        return self.sum / len(self.buffer)
    
    def reset(self):
        """重置滤波器"""
        self.buffer.clear()
        self.sum = 0.0


class LowPassFilter:
    """一阶低通滤波器"""
    
    def __init__(self, alpha: float):
        """
        Args:
            alpha: 滤波系数，越小越平滑 (0 < alpha <= 1)
        """
        self.alpha = alpha
        self.last_output = None
    
    def process(self, value: float) -> float:
        """处理一个新数据点"""
        if self.last_output is None:
            self.last_output = value
        else:
            self.last_output = self.alpha * value + (1 - self.alpha) * self.last_output
        
        return self.last_output
    
    def reset(self):
        """重置滤波器"""
        self.last_output = None


class CombinedFilter:
    """组合滤波器：滑动平均 + 一阶低通"""
    
    def __init__(self, window_size: int, alpha: float):
        self.moving_avg = MovingAverageFilter(window_size)
        self.low_pass = LowPassFilter(alpha)
    
    def process(self, value: float) -> float:
        """处理一个新数据点"""
        # 先滑动平均去噪
        avg_value = self.moving_avg.process(value)
        # 再低通滤波平滑
        filtered_value = self.low_pass.process(avg_value)
        return filtered_value
    
    def reset(self):
        """重置滤波器"""
        self.moving_avg.reset()
        self.low_pass.reset()


# ============================================================
# 阈值判定模块
# ============================================================

@dataclass
class ThresholdState:
    """阈值判定状态"""
    over_count: int = 0           # 连续超限计数
    last_values: List[float] = None  # 历史值（用于趋势计算）
    
    def __post_init__(self):
        if self.last_values is None:
            self.last_values = []


class ThresholdChecker:
    """阈值检查器"""
    
    def __init__(self, warning_threshold: float, alarm_threshold: float,
                 consecutive_count: int, slope_threshold: float):
        self.warning_threshold = warning_threshold
        self.alarm_threshold = alarm_threshold
        self.consecutive_count = consecutive_count
        self.slope_threshold = slope_threshold
        self.state = ThresholdState()
    
    def check(self, value: float) -> int:
        """
        检查值是否超限
        
        Returns:
            0: 正常
            1: 预警
            2: 报警
        """
        # 更新历史值（保留最近10个）
        self.state.last_values.append(value)
        if len(self.state.last_values) > 10:
            self.state.last_values.pop(0)
        
        # 单阈值判定
        is_warning = value >= self.warning_threshold
        is_alarm = value >= self.alarm_threshold
        
        # 趋势判定（计算斜率）
        slope = self._calculate_slope()
        is_rising_fast = abs(slope) > self.slope_threshold
        
        # 综合判定
        if is_alarm or (is_warning and is_rising_fast):
            self.state.over_count += 1
        else:
            self.state.over_count = max(0, self.state.over_count - 1)
        
        # 防抖：连续N次超限才触发
        if self.state.over_count >= self.consecutive_count:
            if is_alarm:
                return 2  # 报警
            elif is_warning:
                return 1  # 预警
        
        return 0  # 正常
    
    def _calculate_slope(self) -> float:
        """计算斜率（变化率）"""
        if len(self.state.last_values) < 2:
            return 0.0
        
        # 使用最近5个点计算线性回归斜率
        n = min(5, len(self.state.last_values))
        values = self.state.last_values[-n:]
        x = np.arange(n)
        
        # 简单线性回归
        slope = np.polyfit(x, values, 1)[0]
        return slope * Config.SAMPLE_RATE  # 转换为每秒变化率
    
    def reset(self):
        """重置状态"""
        self.state = ThresholdState()


# ============================================================
# 状态机模块
# ============================================================

class SystemState(Enum):
    """系统状态"""
    NORMAL = 0      # 正常
    WARNING = 1     # 预警
    ALARM = 2       # 报警
    SHUTDOWN = 3    # 断电


class SystemEvent(Enum):
    """系统事件"""
    NORMAL = 0      # 恢复正常
    WARNING = 1     # 触发预警
    ALARM = 2       # 触发报警
    SHUTDOWN = 3    # 触发断电
    RESET = 4       # 人工复位


class StateMachine:
    """状态机"""
    
    def __init__(self):
        self.current_state = SystemState.NORMAL
        self.state_duration = 0
        self.state_history = []
    
    def process_event(self, event: SystemEvent) -> SystemState:
        """处理事件，返回新状态"""
        old_state = self.current_state
        
        # 状态转换表
        transitions = {
            SystemState.NORMAL: {
                SystemEvent.WARNING: SystemState.WARNING,
                SystemEvent.ALARM: SystemState.ALARM,
                SystemEvent.SHUTDOWN: SystemState.SHUTDOWN,
            },
            SystemState.WARNING: {
                SystemEvent.NORMAL: SystemState.NORMAL,
                SystemEvent.ALARM: SystemState.ALARM,
                SystemEvent.SHUTDOWN: SystemState.SHUTDOWN,
                SystemEvent.RESET: SystemState.NORMAL,
            },
            SystemState.ALARM: {
                SystemEvent.NORMAL: SystemState.NORMAL,
                SystemEvent.SHUTDOWN: SystemState.SHUTDOWN,
                SystemEvent.RESET: SystemState.NORMAL,
            },
            SystemState.SHUTDOWN: {
                SystemEvent.RESET: SystemState.NORMAL,
            },
        }
        
        # 获取新状态
        if event in transitions.get(old_state, {}):
            new_state = transitions[old_state][event]
        else:
            new_state = old_state
        
        # 更新状态
        if new_state != old_state:
            self.state_history.append({
                'time': time.time(),
                'from': old_state,
                'to': new_state,
                'event': event
            })
            self.current_state = new_state
            self.state_duration = 0
        else:
            self.state_duration += 1
        
        return self.current_state
    
    def reset(self):
        """重置状态机"""
        self.current_state = SystemState.NORMAL
        self.state_duration = 0


# ============================================================
# 数据生成模块（模拟传感器数据）
# ============================================================

class SensorSimulator:
    """传感器数据模拟器"""
    
    def __init__(self, duration: float, sample_rate: int):
        self.duration = duration
        self.sample_rate = sample_rate
        self.num_samples = int(duration * sample_rate)
        self.time = np.linspace(0, duration, self.num_samples)
    
    def generate_current_data(self) -> np.ndarray:
        """
        生成模拟电流数据
        
        模拟场景：
        - 正常充电：1-3A
        - 电流逐渐上升：3-5A
        - 异常过流：8-10A（短时）
        - 恢复正常
        """
        t = self.time
        current = np.zeros_like(t)
        
        # 阶段1: 正常充电 (0-10s)
        mask1 = t < 10
        current[mask1] = 2.0 + 0.5 * np.sin(2 * np.pi * 0.1 * t[mask1])
        
        # 阶段2: 电流逐渐上升 (10-20s)
        mask2 = (t >= 10) & (t < 20)
        current[mask2] = 2.0 + 0.3 * (t[mask2] - 10) + 0.5 * np.sin(2 * np.pi * 0.1 * t[mask2])
        
        # 阶段3: 异常过流 (20-25s)
        mask3 = (t >= 20) & (t < 25)
        current[mask3] = 8.0 + 2.0 * np.sin(2 * np.pi * 0.5 * t[mask3])
        
        # 阶段4: 恢复正常 (25-30s)
        mask4 = t >= 25
        current[mask4] = 2.0 + 0.5 * np.sin(2 * np.pi * 0.1 * t[mask4])
        
        # 添加噪声
        noise = np.random.normal(0, 0.1, len(current))
        current += noise
        
        return current
    
    def generate_temperature_data(self) -> np.ndarray:
        """
        生成模拟温度数据
        
        模拟场景：
        - 正常温度：25-30°C
        - 温度逐渐上升：30-45°C
        - 异常高温：50-65°C（短时）
        - 恢复正常
        """
        t = self.time
        temp = np.zeros_like(t)
        
        # 阶段1: 正常温度 (0-10s)
        mask1 = t < 10
        temp[mask1] = 25.0 + 2.0 * np.sin(2 * np.pi * 0.05 * t[mask1])
        
        # 阶段2: 温度逐渐上升 (10-20s)
        mask2 = (t >= 10) & (t < 20)
        temp[mask2] = 25.0 + 2.0 * (t[mask2] - 10) + 2.0 * np.sin(2 * np.pi * 0.05 * t[mask2])
        
        # 阶段3: 异常高温 (20-25s)
        mask3 = (t >= 20) & (t < 25)
        temp[mask3] = 55.0 + 5.0 * np.sin(2 * np.pi * 0.2 * t[mask3])
        
        # 阶段4: 恢复正常 (25-30s)
        mask4 = t >= 25
        temp[mask4] = 25.0 + 2.0 * np.sin(2 * np.pi * 0.05 * t[mask4])
        
        # 添加噪声
        noise = np.random.normal(0, 0.5, len(temp))
        temp += noise
        
        return temp
    
    def generate_leakage_data(self) -> np.ndarray:
        """
        生成模拟漏电数据
        
        模拟场景：
        - 正常：无漏电 (0)
        - 异常：漏电 (1)
        """
        t = self.time
        leakage = np.zeros_like(t)
        
        # 在 20-25s 模拟漏电
        mask = (t >= 20) & (t < 25)
        leakage[mask] = 1.0
        
        return leakage.astype(bool)


# ============================================================
# 可视化模块
# ============================================================

class Visualizer:
    """数据可视化"""
    
    @staticmethod
    def plot_results(time: np.ndarray, 
                    raw_current: np.ndarray, 
                    filtered_current: np.ndarray,
                    raw_temp: np.ndarray,
                    filtered_temp: np.ndarray,
                    leakage: np.ndarray,
                    states: List[SystemState]):
        """绘制结果"""
        fig, axes = plt.subplots(4, 1, figsize=(12, 16), sharex=True)
        
        # 绘制电流
        axes[0].plot(time, raw_current, 'b-', alpha=0.5, label='Raw Current')
        axes[0].plot(time, filtered_current, 'r-', linewidth=2, label='Filtered Current')
        axes[0].axhline(y=Config.CURRENT_WARNING_THRESHOLD, color='orange', 
                       linestyle='--', label=f'Warning ({Config.CURRENT_WARNING_THRESHOLD}A)')
        axes[0].axhline(y=Config.CURRENT_ALARM_THRESHOLD, color='red', 
                       linestyle='--', label=f'Alarm ({Config.CURRENT_ALARM_THRESHOLD}A)')
        axes[0].set_ylabel('Current (A)')
        axes[0].set_title('FireEye Algorithm Prototype - Current Monitoring')
        axes[0].legend(loc='upper right')
        axes[0].grid(True, alpha=0.3)
        
        # 绘制温度
        axes[1].plot(time, raw_temp, 'b-', alpha=0.5, label='Raw Temperature')
        axes[1].plot(time, filtered_temp, 'r-', linewidth=2, label='Filtered Temperature')
        axes[1].axhline(y=Config.TEMP_WARNING_THRESHOLD, color='orange', 
                       linestyle='--', label=f'Warning ({Config.TEMP_WARNING_THRESHOLD}°C)')
        axes[1].axhline(y=Config.TEMP_ALARM_THRESHOLD, color='red', 
                       linestyle='--', label=f'Alarm ({Config.TEMP_ALARM_THRESHOLD}°C)')
        axes[1].set_ylabel('Temperature (°C)')
        axes[1].set_title('Temperature Monitoring')
        axes[1].legend(loc='upper right')
        axes[1].grid(True, alpha=0.3)
        
        # 绘制漏电状态
        axes[2].fill_between(time, 0, leakage.astype(int), alpha=0.5, color='red', label='Leakage Detected')
        axes[2].set_ylabel('Leakage Status')
        axes[2].set_title('Leakage Detection')
        axes[2].legend(loc='upper right')
        axes[2].grid(True, alpha=0.3)
        axes[2].set_ylim(-0.1, 1.1)
        
        # 绘制状态机
        state_values = [s.value for s in states]
        axes[3].plot(time, state_values, 'g-', linewidth=2, label='System State')
        axes[3].fill_between(time, 0, state_values, alpha=0.3, color='green')
        axes[3].set_ylabel('State')
        axes[3].set_xlabel('Time (s)')
        axes[3].set_title('State Machine Output')
        axes[3].set_yticks([0, 1, 2, 3])
        axes[3].set_yticklabels(['Normal', 'Warning', 'Alarm', 'Shutdown'])
        axes[3].legend(loc='upper right')
        axes[3].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig('fireeye_algorithm_results.png', dpi=150, bbox_inches='tight')
        plt.show()
        
        print(f"\n图表已保存到: fireeye_algorithm_results.png")


# ============================================================
# 主程序
# ============================================================

def main():
    """主函数"""
    print("=" * 60)
    print("火眼项目 - PC端算法原型")
    print("=" * 60)
    
    # 创建配置
    config = Config()
    
    # 创建模拟器
    simulator = SensorSimulator(config.DURATION, config.SAMPLE_RATE)
    
    # 生成模拟数据
    print("\n[1/5] 生成模拟传感器数据...")
    raw_current = simulator.generate_current_data()
    raw_temp = simulator.generate_temperature_data()
    leakage = simulator.generate_leakage_data()
    time_array = simulator.time
    
    # 创建滤波器
    print("[2/5] 初始化滤波器...")
    current_filter = CombinedFilter(config.MOVING_AVG_WINDOW, config.LOW_PASS_ALPHA)
    temp_filter = CombinedFilter(config.MOVING_AVG_WINDOW, config.LOW_PASS_ALPHA)
    
    # 创建阈值检查器
    print("[3/5] 初始化阈值检查器...")
    current_checker = ThresholdChecker(
        config.CURRENT_WARNING_THRESHOLD,
        config.CURRENT_ALARM_THRESHOLD,
        config.CONSECUTIVE_COUNT,
        config.SLOPE_THRESHOLD
    )
    temp_checker = ThresholdChecker(
        config.TEMP_WARNING_THRESHOLD,
        config.TEMP_ALARM_THRESHOLD,
        config.CONSECUTIVE_COUNT,
        config.SLOPE_THRESHOLD
    )
    
    # 创建状态机
    print("[4/5] 初始化状态机...")
    state_machine = StateMachine()
    
    # 处理数据
    print("[5/5] 处理数据...")
    filtered_current = []
    filtered_temp = []
    states = []
    
    for i in range(len(time_array)):
        # 滤波
        fc = current_filter.process(raw_current[i])
        ft = temp_filter.process(raw_temp[i])
        filtered_current.append(fc)
        filtered_temp.append(ft)
        
        # 阈值判定
        current_status = current_checker.check(fc)
        temp_status = temp_checker.check(ft)
        
        # 漏电直接触发报警
        if leakage[i]:
            max_status = 2
        else:
            max_status = max(current_status, temp_status)
        
        # 状态机处理
        event = SystemEvent(max_status)
        state = state_machine.process_event(event)
        states.append(state)
    
    # 转换为numpy数组
    filtered_current = np.array(filtered_current)
    filtered_temp = np.array(filtered_temp)
    
    # 打印统计信息
    print("\n" + "=" * 60)
    print("处理结果统计:")
    print("=" * 60)
    
    print(f"\n电流数据:")
    print(f"  原始数据范围: {raw_current.min():.2f} - {raw_current.max():.2f} A")
    print(f"  滤波后范围: {filtered_current.min():.2f} - {filtered_current.max():.2f} A")
    print(f"  预警次数: {sum(1 for s in states if s == SystemState.WARNING)}")
    print(f"  报警次数: {sum(1 for s in states if s == SystemState.ALARM)}")
    
    print(f"\n温度数据:")
    print(f"  原始数据范围: {raw_temp.min():.2f} - {raw_temp.max():.2f} °C")
    print(f"  滤波后范围: {filtered_temp.min():.2f} - {filtered_temp.max():.2f} °C")
    print(f"  预警次数: {sum(1 for s in states if s == SystemState.WARNING)}")
    print(f"  报警次数: {sum(1 for s in states if s == SystemState.ALARM)}")
    
    print(f"\n漏电检测:")
    print(f"  漏电事件数: {leakage.sum()}")
    
    print(f"\n状态机:")
    print(f"  正常: {sum(1 for s in states if s == SystemState.NORMAL)} 次采样")
    print(f"  预警: {sum(1 for s in states if s == SystemState.WARNING)} 次采样")
    print(f"  报警: {sum(1 for s in states if s == SystemState.ALARM)} 次采样")
    print(f"  断电: {sum(1 for s in states if s == SystemState.SHUTDOWN)} 次采样")
    
    # 可视化
    print("\n正在生成可视化图表...")
    Visualizer.plot_results(
        time_array, raw_current, filtered_current,
        raw_temp, filtered_temp, leakage, states
    )
    
    print("\n算法原型验证完成！")
    print("=" * 60)


if __name__ == "__main__":
    main()
