#!/usr/bin/env python3
# -*- coding: utf-8 -*-


def calculate_improvement(
    metric_name,
    algorithm_names,
    metric_values,
    fbm_name="PBS",
):
    """
    按照公式计算：

        (DeepHir指标 - FBM指标) / DeepHir指标

    这里：
        DeepHir指标 = 各个BMS阈值对应的指标
        FBM指标 = PBS对应的指标
    """

    if len(algorithm_names) != len(metric_values):
        raise ValueError(
            f"{metric_name}：算法名称数量与数据数量不一致"
        )

    if fbm_name not in algorithm_names:
        raise ValueError(
            f"{metric_name}：没有找到FBM算法 {fbm_name}"
        )

    fbm_index = algorithm_names.index(fbm_name)
    fbm_value = metric_values[fbm_index]

    print("=" * 70)
    print(f"指标：{metric_name}")
    print(f"FBM基准：{fbm_name} = {fbm_value}")
    print()
    print(
        f"{'DeepHir算法':<15}"
        f"{'DeepHir指标':>15}"
        f"{'FBM指标':>15}"
        f"{'计算结果':>15}"
        f"{'百分比':>12}"
    )
    print("-" * 70)

    results = {}

    for algorithm, deephir_value in zip(
        algorithm_names,
        metric_values,
    ):
        # PBS是FBM自身，不需要作为DeepHir进行比较
        if algorithm == fbm_name:
            continue

        if deephir_value == 0:
            print(
                f"{algorithm:<15}"
                f"{deephir_value:>15.6f}"
                f"{fbm_value:>15.6f}"
                f"{'无法计算':>15}"
                f"{'--':>12}"
            )
            continue

        improvement = (
            deephir_value - fbm_value
        ) / deephir_value

        results[algorithm] = improvement

        print(
            f"{algorithm:<15}"
            f"{deephir_value:>15.6f}"
            f"{fbm_value:>15.6f}"
            f"{improvement:>15.6f}"
            f"{improvement * 100:>11.2f}%"
        )

    print()

    return results


def main():
    algorithm_names = [
        "BMS-0.2M",
        "BMS-0.5M",
        "BMS-1.0M",
        "BMS-2.0M",
        "BMS-4.0M",
        "PBS",
    ]

    # 丢包数据
    packet_loss = [66016.0, 31278.0, 20834.0, 20909.0, 20909.0, 18341.0]

    # 平均FCT数据
    avg_fct = [5.566, 3.973, 2.669, 2.619, 2.619, 1.588]

    loss_results = calculate_improvement(
        metric_name="Packet Loss",
        algorithm_names=algorithm_names,
        metric_values=packet_loss,
        fbm_name="PBS",
    )

    fct_results = calculate_improvement(
        metric_name="Average FCT",
        algorithm_names=algorithm_names,
        metric_values=avg_fct,
        fbm_name="PBS",
    )

    print("=" * 70)
    print("结果列表")
    print()

    print("Packet Loss：")
    print([
        round(loss_results[name] * 100, 2)
        for name in algorithm_names
        if name != "PBS"
    ])

    print()

    print("Average FCT：")
    print([
        round(fct_results[name] * 100, 2)
        for name in algorithm_names
        if name != "PBS"
    ])


if __name__ == "__main__":
    main()