#!/usr/bin/env python3
"""
贝叶斯优化器封装：使用 bayesian-optimization 库。
"""

from bayes_opt import BayesianOptimization

def create_optimizer(target_function, space_dict, random_state=42):
    """
    创建一个贝叶斯优化器。
    target_function: 目标函数，接受关键字参数 (P_angle, P_gyro, P_lqr_u, I_lqr_u) 并返回一个浮点值。
    space_dict: {'参数名': (下界, 上界)}
    返回: BayesianOptimization 对象（已绑定目标函数）
    """
    pbounds = {name: bounds for name, bounds in space_dict.items()}
    optimizer = BayesianOptimization(
        f=target_function,          # 直接绑定目标函数
        pbounds=pbounds,
        random_state=random_state,
        allow_duplicate_points=True
    )
    return optimizer