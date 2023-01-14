# Mbed CAN Motor Driver for spirit

[spirit](https://github.com/yutotnh/spirit) を用いた Mbed の モータードライバ制御プログラム

下の図の赤のノードに相当します

```mermaid
flowchart TB
    classDef Peripheral fill:#EA3323,fill-opacity:0.5

    A[Controller]
    A -- CAN --> B["Peripheral\n(Motor Driver)"]:::Peripheral --> B2[Motor]
    A -- CAN --> C["Peripheral\n(Motor Driver)"]:::Peripheral --> C2[Motor]
    A -- CAN --> D["Peripheral\n(Motor Driver)"]:::Peripheral --> D2[Motor]
    A -- CAN --> E["Peripheral\n(Motor Driver)"]:::Peripheral --> E2[Motor]
```

制御対象の回路は、 [yutotnh/CAN_H-Bridge_Solo_MD](https://github.com/yutotnh/CAN_H-Bridge_Solo_MD) です
