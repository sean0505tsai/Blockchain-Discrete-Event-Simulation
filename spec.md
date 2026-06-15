# Bitcoin Network Simulation Specification Version B

MMnL model of modeling blockchain
Ver.B Proof-of-Work (PoW) competition
這是一個離散事件模擬，使用下一事件推進法來模擬比特幣網路的運作  
模擬真實比特幣的運作方式，礦工競爭來獲得區塊獎勵，但沿用論文的1000個伺服器和144個區塊的設定。

## 系統規格

- Mean Inter-arrival Time: 1/3.147 (seconds per transaction) *[可從input讀取]*
- Mean Service Time per Server: 600000 (seconds) *[可從input讀取]*
- Number of Servers: 1000
- Number of Blocks to Simulate: **動態読取** (N + 10，玫機10 + 統計N) *[可從input讀取]*
- Mempool Capacity: **動態** (読input) *[可從input讀取]*
- Max Transactions per Block: **動態** (読input) *[可從input讀取]*
- Queue Policy: First-Come-First-Serve (FCFS)
- **Block Generation Trigger: when the miner with the lowest service time completes a block**

## 系統架構及行為

incoming transactions -> waiting queue(capacity: 3000) -> servers(1000 parallel) -> new block generation -> output

### Initialization

t=0時，系統開始運行（冷啟動處理空區塊）:

- waiting queue為空
- 所有礦工(server)初始設為BUSY狀態（處理空區塊）
- 已完成的區塊數量為0
- `time_last_event` = 0（用於計算區間統計）
- `last_block_time` = 0（上一區塊生成時刻）
- 設定交易的到達時間，該時間以指數分布mean=1/3.147秒設定
- 以迴圈為所有礦工以指數分布mean=600000設定第一次完工時間並更新至 `time_next_event[2]`
- **說明**：冷啟動時礦工處理空區塊，這與真實比特幣系統中礦工持續運作的行為一致

### 主迴圈&推進機制

- 本系統採用「下一事件時間推進法 (Next-Event Time Advance)」，嚴禁使用逐秒/固定步長輪詢 (Polling)。
- 系統必須維護一個未來事件列表 (Future Event List, FEL) 陣列 `time_next_event`，長度為 2：
  - `time_next_event[1]`: 下一次交易抵達時間 (Arrival)
  - `time_next_event[2]`: 1000個礦工中，最早完成交易處理的時間 (Departure)
- **區塊生成由 Departure 事件觸發：** 當最早完成交易處理的礦工完成區塊時，觸發區塊生成事件

#### 執行迴圈

當已完成的區塊數量小於 154 時，持續執行以下步驟：

**區塊編號說明**：
- 總模擬：154個區塊（區塊 #1~#154）
- 暖機階段：前10個區塊（#1~#10），不計入統計
- 統計範圍：區塊 #11~#154，重新編號為 1~144

1. **時間推進 (Timing Function):**
   - 找出 `time_next_event` 陣列中數值最小且大於 0 的時間點，將系統虛擬時鐘 `sim_time` 直接跳躍至該時間。
   - 依據與上一次事件的時間差 (`sim_time - time_last_event`)，更新全網礦工忙碌時間面積（用於計算 Utilization）與佇列長度面積統計。更新後將 `time_last_event` 設為 `sim_time`。

2. **事件分流處理 (Event Handlers):**

   - **狀況 1：若下一個事件為「交易抵達 (Arrival)」：**
     - 如果區塊 >= #11（統計範圍）：將此交易計入 `total_arrived_transactions` 統計。
     - 依據卜瓦松分佈 (Mean = mean_interarrival) 產生隨機數，安排下一次交易抵達時間並更新至 `time_next_event[1]`。
     - 檢查mempool是否已滿（當前佇列長度 >= queue_capacity）：
       - 如果未滿：將當前交易加入等待佇列 (Mempool)。
       - 如果已滿：直接丟棄該交易，但只在區塊 #11~#154 時計入 `dropped_transactions` 統計（前10個區塊丟棄的交易不記錄）。
  
   - **狀況 2：若下一個事件為「礦工完成交易處理 (Departure)」：**
     - 觸發區塊生成事件
     - **區塊打包**：從mempool中取出最多 `max_transactions_per_block` 條交易，記錄取出的交易數量作為該區塊的交易數
       - `transactions_in_block = min(num_in_mempool, max_transactions_per_block)`
       - 更新mempool：`num_in_mempool -= transactions_in_block`
     - **所有礦工重新分配**：無論隊列是否有交易（允許空區塊），設定所有礦工回到BUSY狀態（`num_busy_servers` = 1000），並以迴圈為每個礦工獨立設定指數分布mean=600000隨機產生服務時間，記錄各礦工在本區塊時間區間內的忙碌時長
     - 更新 `time_next_event[2]` 為全網最早完工

### Transaction Arrival

- Transactions arrive following a Poisson process with a mean inter-arrival rate of 3.147 arrivals per second (interval time: 1/3.147s).

### Waiting Queue (Mempool) and Completed Transaction Buffer

- Capacity: `queue_capacity`（從input讀取，預設1000000）
- Policy: First-Come-First-Serve (FCFS)
- **丟棄機制**：
  - 當新交易抵達時，檢查 `num_in_mempool` 是否 < queue_capacity
  - 如果 < queue_capacity：交易進入等待佇列 (Mempool)
  - 如果 ≥ queue_capacity：新交易被丟棄
    - 如果區塊 #1~#10（暖機）：丟棄但不記錄
    - 如果區塊 #11~#154（統計）：丟棄並計入 `dropped_transactions` 統計
- **丟棄率 (Drop Rate)**：`區塊#11~#154的丟棄交易總數 / 區塊#11~#154的到達交易總數 × 100%`
- **區塊取交易機制**：
  - 每個區塊最多取出 `max_transactions_per_block` 條交易
  - 實際取出數 = min(num_in_mempool, max_transactions_per_block)
  - 取出後，mempool數量相應減少（而非清空整個queue）

### Miner (Servers)

- Number of Servers: 1000
- Service Time of Each Server: Exponentially distributed with a mean of 600000 seconds per transaction。在取得交易後設定一個服務時間，並在該時間結束後完成交易處理。
- **礦工狀態管理**：
  - `num_busy_servers`：記錄目前忙碌中的礦工數量
  - `server_state[i]`：第i個礦工的狀態（BUSY 或 IDLE）
  - `server_finish_time[i]`：第i個礦工的完工時間（IDLE時設為無限大 1.0e+30）
  - **狀態轉換**：
    - BUSY → 完工時觸發Departure事件 → 轉為IDLE
    - IDLE → 區塊生成事件時被分配新區塊 → 轉為BUSY
  - **忙碌時間記錄**：為計算Utilization，需記錄各礦工在每個區塊時間區間內的忙碌時長（sum of (min(server_finish_time[i], block_end_time) - max(server_start_time[i], block_start_time)))）
- Departure 事件流程：
  1. 區塊打包與更新：
     - 從等待佇列 (Mempool) 中取出最多 `max_transactions_per_block` 條交易
     - 記錄取出的交易數，作為該產出區塊的「區塊交易數 (Number of Transactions in Block)」
     - 更新mempool大小：`num_in_mempool -= transactions_in_block`
     - **注意**：mempool不再完全清空，而是持續保留未被取出的交易，等待下一個區塊

  2. 統計與紀錄：
     - 區塊數量 +1。
     - **區塊生成時間間隔計算**：
       - 第一個區塊：`interval[1] = block_1_time - 0`
       - 後續區塊k（k ≥ 2）：`interval[k] = block_k_time - block_{k-1}_time`
     - **伺服器利用率計算**：
       $$utilization[k] = \frac{\sum_{i=1}^{1000} \text{busy\_time}[i] \text{ in block k interval}}{1000 \times block\_duration} \times 100\%$$
       其中 `block_duration = block_k_time - block_{k-1}_time`（第一個區塊則為 `block_1_time - 0`）
       因為所有礦工均獲得新區塊時重新設為BUSY，實際利用率應接近 100%（但非硬編碼，由模擬自然得出）

  3. 全網算力重新分配 (Event Preemption)：
     - 使用迴圈為 1000 個礦工重新且獨立地抽取下一次的服務時間（指數分布 mean=600000）。
     - 將所有礦工狀態轉為 BUSY（開始處理下一批區塊）
     - 找出這 1000 人中新的最早完工時間，並更新至 `time_next_event[2]`。

### Block Generation

- **區塊生成觸發機制**： 當最早完成交易處理的礦工完成區塊時，觸發區塊生成事件
- 記錄並輸出該區塊的：交易數量、生成間隔、礦工利用率，開始累積下一個區塊。
- **統計丟棄率**：在模擬結束時計算 `drop_rate = dropped_transactions / total_arrived_transactions × 100%`

## 輸入(mmnl_pow.in)

格式: `mean_interarrival mean_service queue_capacity max_transactions_per_block num_blocks_to_simulate`

例如: `0.3177 600000 1000000 3000 2016`

- Mean Inter-arrival Time: 0.3177 (seconds per transaction, 相當於 1/3.147)
- Mean Service Time per Server: 600000 (seconds)
- Queue Capacity (Mempool): 1000000 (transactions, 可動態調整)
- Max Transactions per Block: 3000 (交易上限)
- Num Blocks to Simulate: 2016 (統計區塊數，不含暖機)

## 輸出檔案

統計資訊將輸出至以下四個檔案：

**區塊編號約定**（以輸入值2016為例）：
- 實際模擬區塊編號：#1~#2026
- 暖機區塊：#1~#10 (自動添加，不輸出)
- 統計區塊：#11~#2026，在 .out 檔案中重新編號為 1~2016
- **mmnl_pow.out**：只包含暖機後10個區塊的詳細資訊 + 統計摘要
- **num_transactions_block.out、block_generation_interval.out、utilization.out**：只包含統計區塊 1~2016

### mmnl_pow.out (主報告檔案)

格式進了暖機10區塊後漄的區塊資訊，加上統計摘要：
```
Bitcoin Network Simulation (MMnL Model)

Mean inter-arrival time: 0.318 seconds
Mean service time: 600000.000 seconds
Number of servers (miners): 1000
Number of blocks to simulate: 2016
Mempool capacity: 1000000

#11, Transactions: 3000, Interval: 1359.6 s, Utilization: 99.9%
#12, Transactions: 3000, Interval: 1380.0 s, Utilization: 99.7%
...
#2026, Transactions: 6, Interval: 1.8 s, Utilization: 100.0%

===== SIMULATION SUMMARY =====

Total blocks generated: 2016
Total transactions completed: XXXX
Avg. Number of Transactions/Block: 1660.3
Mean Block Generation Interval: 528.2 seconds
Mean Utilization of Servers: 99.9%
Total simulation time: XXXXXX seconds
Max Mempool Size: 1000000

Transaction Statistics:
Total arrived transactions: XXXXX
Dropped transactions: XXXX
Drop rate: X.XXXXXX%
```

**詳細說明**：

- mmnl_pow.out 中僅輸出暖機後的區塊 (#11 起)
- SIMULATION SUMMARY 中的所有統計數據僅計算統計區塊 (#11~#N+10) 的數據
- Max Mempool Size 是整個模擬中 mempool 超過暖機後的最大優化值

### num_transactions_block.out (統計區塊1~N)

格式: `block_number, number_of_transactions_in_block`  
example:
```
1, 3000
2, 3000
3, 2618
4, 784
5, 2283
```

### block_generation_interval.out (統計區塊1~N)

格式: `block_number, block_generation_interval(in_seconds)`  
example:
```
1, 1359.6
2, 1380.0
3, 31.8
4, 256.1
5, 754.4
```

### utilization.out (統計區塊1~N)

格式: `block_number, utilization_percentage`  
example:
```
1, 99.87
2, 99.69
3, 99.99
4, 99.94
5, 99.93
```

## print

### 模擬中逐個區塊完成時，印出該區塊的資訊 (所有154個區塊)

- Block Number (#1 ~ #154，含暖機塊)
- Number of Transactions in Block
- Block Generation Interval (in seconds)
- Utilization of Servers for Block (in percentage)

格式: `#X, Transactions: Y, Interval: Z s, Utilization: W%`

例如：
```
#1, Transactions: 3000, Interval: 1109.9 s, Utilization: 99.8%
#2, Transactions: 3000, Interval: 1113.2 s, Utilization: 99.9%
...
#10, Transactions: 1526, Interval: 57.0 s, Utilization: 100.0%
#11, Transactions: 3000, Interval: 1359.6 s, Utilization: 99.9%
...
#154, Transactions: 6, Interval: 1.8 s, Utilization: 100.0%
```

### 模擬結束後，印出整體統計資訊

- Avg. Number of Transactions/Block
- Mean Block Generation Interval
- Mean Utilization of Servers
- **Transaction Statistics**:
  - Total arrived transactions
  - Dropped transactions
  - Drop rate (percentage)