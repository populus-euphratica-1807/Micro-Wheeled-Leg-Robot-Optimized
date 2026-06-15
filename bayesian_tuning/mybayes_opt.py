#!/usr/bin/env python3
"""
贝叶斯优化器封装
"""

from bayes_opt import BayesianOptimization

def create_optimizer(target_function, space_dict, random_state=42):
    pbounds = {name: bounds for name, bounds in space_dict.items()}
    optimizer = BayesianOptimization(
        f=target_function,
        pbounds=pbounds,
        random_state=random_state,
        allow_duplicate_points=True
    )
    return optimizer