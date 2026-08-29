## 1. bsp_can接口使用说明

## 2. 之前踩过的坑

## 3. 当前cubemx默认配置和原因

| key                 | value                   | 原因                                       |
| ------------------- | ----------------------- | ------------------------------------------ |
| Frame Format        | CAN_FRAME_FORMAT_FD_BRS | CAN_FRAME_FORMAT_FD_BRS模式下可以发送3种帧 |
| Auto Retransmission | Enable                  | 可以要                                     |
| Transmit Pause      | Disable                 | 减小延迟                                   |
| Protocol Exception  | Enable                  | 可以提高兼容性                             |

| key                               | value   | 原因                                                                                                                                     |
| --------------------------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Time Triggered Communication Mode | Disable | 没必要                                                                                                                                   |
| Automatic Bus-Off Management      | Enable  | 不然就要手动恢复，之前吃过亏                                                                                                             |
| Automatic Wake-Up Mode            | Disable | 不会休眠，没必要                                                                                                                         |
| Automatic Retransmission          | Enable  | 可以要                                                                                                                                   |
| Receive Fifo Locked Mode          | Disable | Disable情况下，FIFO满时新数据覆盖最旧的数据（控制情况下新数据价值高于旧数据），fdcan没有这个配置选项，默认情况下和bxcan的Disable行为一致 |
| Transmit Fifo Priority            | Enable  | 和fdcan的fifo保持一致                                                                                                                    |

## 4. can的状态：错误码，错误计数等使用方式
