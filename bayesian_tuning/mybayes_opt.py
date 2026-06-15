#!/usr/bin/env python3
"""
贝叶斯优化器封装，定义自定义中断异常。
"""

from bayes_opt import BayesianOptimization


class ExperimentAbortError(Exception):
    """实验连续失败导致的终止异常"""
    pass


def create_optimizer(target_function, space_dict, random_state=42, verbose=0):
    pbounds = {name: bounds for name, bounds in space_dict.items()}
    optimizer = BayesianOptimization(
        f=target_function,
        pbounds=pbounds,
        random_state=random_state,
        allow_duplicate_points=True,
        verbose=verbose
    )
    return optimizer