# Blockchain-Discrete-Event-Simulation

這是一個基於離散事件模擬的比特幣 PoW 共識機制與 Mempool 壅塞動態分析專案，旨在模擬比特幣網路中 1000 個礦工的 PoW 競爭與區塊生成流程，並輸出每個統計區塊的相關數據。

## 專案結構

```

report/
├── report.md            # 專案報告
└── images/             # 報告中使用的圖片

src/
├── mmnl_pow.c          # 模擬器核心程式碼
    ├── mmnl_pow.h          # 模擬器標頭檔
    ├── lcgrand.c          # 隨機數生成器
    ├── lcgrand.h          # 隨機數生成器標頭檔
    ├── mmnl_pow.in          # 模擬器參數輸入檔案

plot/
├── plot.py              # 統計數據繪圖程式碼
└── bitcoin_nov_17_2018/  # 比特幣網路真實數據 (2018/11/17)
    ├── block_generation_interval.png   # 區塊生成間隔圖
    ├── block_size.png                   # 區塊大小圖
    └── num_of_tx_in_block.png          # 區塊交易數量圖

└── sim_144_blocks/  # 模擬器輸出數據 (模擬 144 個區塊)
    ├── block_generation_interval.png   # 區塊生成間隔圖
    ├── block_size.png                   # 區塊大小圖
    └── num_of_tx_in_block.png          # 區塊交易數量圖

requirements.txt          # Python 依賴套件列表
spec.md                   # 模擬器規格說明文件
README.md                 # 專案說明文件

```

## 執行說明

### 模擬器運作

1. 編譯模擬器程式碼：

   ```bash
   cd src
   gcc -o mmnl_pow mmnl_pow.c lcgrand.c
   ```

2. 執行模擬器：

    ```bash
    mmnl_pow
    ```

3. 模擬器參數設定：

    模擬器的參數設定位於 `src/mmnl_pow.in`，使用者可以根據需要修改該檔案中的參數值來進行不同情境的模擬。  
    參數格式為：  
    `mean_interarrival mean_service_time queue_capacity max_block_size num_blocks_to_simulate`

    參數說明:

### 輸出數據製圖

1. 安裝 Python 依賴套件：

   ```bash
   pip install -r requirements.txt
   ```

2. 執行繪圖程式碼：

    ```bash
    python plot/plot.py
    ```