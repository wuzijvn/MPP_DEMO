# Stage06 samples

基础 demo 默认使用模拟队列，不强制需要真实码流。
如果要把本阶段扩展为真实 V4L2 M2M decode，需要准备 Annex B H.264/H.265 elementary stream，并根据目标驱动确认 OUTPUT fourcc、CAPTURE format、buffer count、resolution-change 事件处理方式。
