# Blockchain-Discrete-Event-Simulation

這是一個基於離散事件模擬的比特幣 PoW 共識機制與 Mempool 壅塞動態分析專案，旨在模擬比特幣網路中 1000 個礦工的 PoW 競爭與區塊生成流程，並輸出每個統計區塊的相關數據。  
本專案使用GitHub Copilot進行程式碼輔助撰寫，並且在模擬器的設計與實作過程中參考了原論文的模型架構，對於運作機制的部分則進行了修改，使其更接近真實比特幣系統的運作。

## 專案結構

```
Blockchain-Discrete-Event-Simulation/
├── LICENSE                              # 專案開源協議
├── README.md                            # 專案說明文件（本檔案）
├── spec.md                              # 模擬器規格說明文件
├── requirements.txt                     # Python 套件列表

├── src/                                 # 模擬器核心程式碼
│   ├── mmnl_pow.c                      # 離散事件模擬器核心程式碼
│   ├── mmnl_pow.h                      # 模擬器標頭檔
│   ├── lcgrand.c                       # 隨機數生成器實現（線性同餘法）
│   ├── lcgrand.h                       # 隨機數生成器標頭檔
│   └── mmnl_pow.in                     # 模擬器參數輸入檔案

├── tools/                               # 輔助工具程式
│   ├── average.py                      # 統計分析工具：計算模擬輸出的平均值
│   └── get_block_data.py                # 數據爬蟲工具：從 Mempool.space API 獲取真實比特幣區塊數據

├── plot/                                # 資料可視化
│   ├── plot.py                          # 繪圖程式碼（生成統計圖表）
│   ├── bitcoin_nov_17_2018/            # 真實比特幣數據對比（2018/11/17）
│   │   ├── block_generation_interval.png   # 真實區塊生成間隔分佈圖
│   │   ├── block_size.png                  # 真實區塊大小分佈圖
│   │   └── num_of_tx_in_block.png         # 真實區塊交易數量分佈圖
│   └── sim_144block/                   # 模擬器輸出數據對比（144 個區塊）
│       ├── block_generation_interval.png   # 模擬區塊生成間隔分佈圖
│       ├── block_size.png                  # 模擬區塊大小分佈圖
│       └── num_of_tx_in_block.png         # 模擬區塊交易數量分佈圖

├── outputs/                             # 模擬和爬蟲輸出結果
│   ├── output.txt                      # 模擬器輸出數據
│   └── output_real_bitcoin.txt         # 真實比特幣區塊數據

└── report/                              # 專案報告和分析
    ├── report.md                       # 完整研究報告
    └── images/                         # 報告中使用的圖片

```

## 報告內容

請參閱 `report/report.md` 中的[完整報告](https://github.com/sean0505tsai/Blockchain-Discrete-Event-Simulation/blob/main/report/report.md):  
https://github.com/sean0505tsai/Blockchain-Discrete-Event-Simulation/blob/main/report/report.md

## 執行說明

### 1. 主程式：模擬器運作

#### 1.1 編譯模擬器

在 `src` 目錄下編譯 C 程式：

```bash
cd src
gcc -o mmnl_pow mmnl_pow.c lcgrand.c
```

編譯完成後會生成 `mmnl_pow` 可執行檔。

#### 1.2 執行模擬器

在編譯完成後執行模擬器：

```bash
./mmnl_pow
```

或在 Windows 上：

```cmd
mmnl_pow.exe
```

#### 1.3 模擬器參數設定

模擬器的參數配置位於 `src/mmnl_pow.in`，使用者可修改該檔案中的參數值進行不同情景的模擬。

**參數格式**：
```
mean_interarrival mean_service_time queue_capacity max_block_size num_blocks_to_simulate
```

**參數說明**：

| 參數 | 單位 | 說明 |
|------|------|------|
| `mean_interarrival` | 秒 | 交易到達平均間隔時間（指數分布） |
| `mean_service_time` | 秒 | 單一礦工完成一輪 PoW 競爭的平均時間（指數分布） |
| `queue_capacity` | vB | Mempool 容量上限（虛擬位元組）|
| `max_block_size` | vB | 每個區塊可打包交易總大小上限（虛擬位元組） |
| `num_blocks_to_simulate` | 個 | 統計區塊數（暖機區塊另外計算，不含在內） |

**範例**：
```
0.3177 600000 300000000 1000000 144
```

**模擬器固定值**：
- 礦工數量：1000 個
- 暖機區塊數：10 個（模擬初始化階段，結果不統計）
- 實際模擬區塊數：num_blocks_to_simulate + 10

#### 1.4 模擬器輸出

模擬器完成後會生成輸出檔案 `output.txt`，格式為：

```
block_number, block_interval, num_of_tx_in_block, block_size
```

**欄位說明**：
- `block_number`：區塊編號
- `block_interval`：與前一個區塊的生成時間差（秒）
- `num_of_tx_in_block`：該區塊包含的交易筆數
- `block_size`：該區塊的實體大小（MiB）

### 2. 工具程式

#### 2.1 統計分析工具：`average.py`

用於分析模擬器或爬蟲輸出的數據檔案，計算各項統計指標。

**功能**：
- 讀取輸出檔案（CSV 格式）
- 計算區塊生成間隔、交易筆數、區塊大小的平均值
- 輸出格式化的統計報告

**使用方法**：

```bash
python tools/average.py
```

**預設分析文件**：`outputs/output_real_bitcoin.txt`

**修改分析目標**：編輯 `tools/average.py` 最後的 `target_file` 變數：

```python
if __name__ == "__main__":
    target_file = "../outputs/output.txt"  # 改為要分析的文件路徑
    analyze_bitcoin_txt(target_file)
```

**輸出範例**：
```
==============================================
 歷史數據檔案: 'outputs/output_real_bitcoin.txt' 讀取解析成功
==============================================
 總觀測區塊數 : 145 個
 出塊間隔時間 (欄位 2) 平均值 : 563.24 秒
 單區塊交易數量 (欄位 3) 平均值 : 1852.36 筆
 單區塊實體大小 (欄位 4) 平均值 : 0.9863 MiB
==============================================
```

#### 2.2 數據爬蟲工具：`get_block_data.py`

從公開的 Mempool.space API 爬取真實比特幣區塊數據，用於與模擬結果進行對比分析。

**功能**：
- 連接 Mempool.space 官方 API
- 爬取指定高度範圍內的區塊數據
- 計算區塊生成時間間隔
- 輸出規範化的數據文件

**API 來源**：https://mempool.space/api

**使用方法**：

```bash
python tools/get_block_data.py
```

**修改爬蟲參數**：編輯 `tools/get_block_data.py` 中的配置常數：

```python
API_BASE = "https://mempool.space/api"  # API 基礎 URL
START_HEIGHT = 550370                   # 起始區塊高度
END_HEIGHT = 550514                     # 結束區塊高度
OUTPUT_FILE = Path(...) / "outputs" / "output_real_bitcoin.txt"  # 輸出文件路徑
```

**輸出格式**：與模擬器輸出相同，便於對比分析

```
block_number, block_interval, num_of_tx_in_block, block_size
```

**注意事項**：
- 需要網路連接以訪問 Mempool.space API
- API 可能有速率限制，爬蟲內置了瀏覽器偽裝頭以避免被阻擋
- 爬取大量區塊可能耗時較長，請耐心等待

### 3. 數據可視化

#### 3.1 安裝依賴

```bash
pip install -r requirements.txt
```

#### 3.2 執行繪圖程式

```bash
python plot/plot.py
```

繪圖程式將自動讀取 `outputs/` 目錄中的數據檔案，並在 `plot/` 目錄中生成以下圖表：
- 區塊生成間隔分佈圖
- 區塊大小分佈圖
- 區塊交易數量分佈圖

### 4. 完整工作流程

1. **配置參數**：編輯 `src/mmnl_pow.in` 設定模擬參數
2. **編譯和執行模擬**：
   ```bash
   cd src
   gcc -o mmnl_pow mmnl_pow.c lcgrand.c
   ./mmnl_pow
   cd ..
   ```
3. **爬取真實數據**（非必要）：
   ```bash
   python tools/get_block_data.py
   ```
4. **查看統計分析** (非必要)：
   ```bash
   python tools/average.py
   ```
5. **生成圖表**：
   ```bash
   pip install -r requirements.txt
   python plot/plot.py
   ```