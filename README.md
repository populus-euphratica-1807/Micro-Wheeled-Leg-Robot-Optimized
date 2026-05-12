本项目基于 Mu Shibo 的开源轮足机器人进行重构。
初版主要工作： 
新增 manual_pid_speed_override 标志、基础发送函数（sendToWeb、SerialAndWeb），并在 loop() 中加入电池低压保护；Web 端新增 get_params 模式，支持在线读取全部 PID 及滤波器参数。

初版重在搭建新一代软件框架，最终稳定版请见 main 分支。
