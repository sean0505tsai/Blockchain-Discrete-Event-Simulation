# Bitcoin Network Simulation Spec (Implementation Ready)

版本: B (PoW competition)
模型: 離散事件模擬 (Next-Event Time Advance)
核心原則: mempool 與區塊打包皆以交易大小 (vB) 為基礎，不以交易筆數上限為基礎。

## 1. 目標與範圍

模擬比特幣網路中 1000 個礦工的 PoW 競爭與區塊生成流程，輸出每個統計區塊的:

- 區塊生成間隔 (秒)
- 區塊交易筆數
- 區塊交易總大小
- 交易丟棄統計

## 2. 輸入

檔案: mmnl_pow.in  
格式:  
`mean_interarrival mean_service_time queue_capacity max_block_size num_blocks_to_simulate`

範例:
0.3177 600000 300000000 1000000 2016

參數說明:

- mean_interarrival: 交易到達平均間隔秒數
- mean_service_time: 單一礦工每輪競爭完成時間之指數分布平均值 (秒)
- queue_capacity: mempool 容量上限 (vB)
- max_block_size: 每個區塊可打包交易總大小上限 (vB)
- num_blocks_to_simulate: 統計區塊數 N (不含暖機)

固定值:

- num_servers = 1000
- warmup_blocks = 10
- total_blocks = N + warmup_blocks

## 3. 狀態與資料結構

必要狀態變數:

- sim_time: 目前模擬時間
- time_last_event: 上次事件時間
- last_block_time: 上一個區塊生成時間
- block_count_total: 已生成區塊數 (含暖機)

事件時間:

- time_next_arrival
- time_next_departure

mempool:

- queue: FCFS，元素至少包含 tx_size_vb
- mempool_current_size_vb
- max_mempool_size_vb (整個模擬期間最大值)

礦工:

- server_finish_time[1..1000]
- block_start_time (當前區塊競爭開始時間)

統計 (僅統計區塊 #11 開始):

- total_arrived_transactions
- dropped_transactions
- total_completed_transactions
- sum_block_interval
- sum_block_tx_count
- sum_block_size_vb

每區塊記錄 (供輸出):

- block_interval_sec
- num_of_tx_in_block
- block_size_vb

## 4. 初始化

1. sim_time = 0
2. time_last_event = 0
3. last_block_time = 0
4. block_count_total = 0
5. mempool 為空，mempool_current_size_vb = 0
6. 產生 time_next_arrival = sim_time + Exp(mean_interarrival)
7. 對 1000 位礦工各自產生 finish time:
   - server_finish_time[i] = sim_time + Exp(mean_service_time)
8. time_next_departure = min(server_finish_time)

說明:

- 冷啟動允許空區塊，符合礦工持續競爭的設定。

## 5. 事件推進主迴圈

當 block_count_total < total_blocks 時重複:

1. 取下一事件時間
   - next_time = min(time_next_arrival, time_next_departure)
   - sim_time = next_time

2. 更新時間面積統計
   - time_since_last = sim_time - time_last_event
   - time_last_event = sim_time

3. 分流處理
   - 若 time_next_arrival <= time_next_departure，處理 Arrival
   - 否則處理 Departure

備註:

- 需固定 tie-break 規則避免非決定性，建議 Arrival 優先。

## 6. Arrival 事件

1. 安排下一次到達:
   - time_next_arrival = sim_time + Exp(mean_interarrival)

2. 產生交易大小:
   - tx_size_vb = generate_dynamic_tx_size()

3. 若目前已進入統計區塊 (block_count_total >= warmup_blocks):
   - total_arrived_transactions += 1

4. 檢查容量後入隊或丟棄:
   - 若 mempool_current_size_vb + tx_size_vb <= queue_capacity:
     - queue.push(tx_size_vb)
     - mempool_current_size_vb += tx_size_vb
     - max_mempool_size_vb = max(max_mempool_size_vb, mempool_current_size_vb)
   - 否則:
     - 若在統計期，dropped_transactions += 1

## 7. Departure 事件 (區塊生成)

1. 計算區塊間隔:
   - block_interval_sec = sim_time - last_block_time
   - last_block_time = sim_time

2. 依 FCFS 打包交易，受 max_block_size 限制:
   - tx_count = 0
   - block_size_vb = 0
   - while queue 非空:
     - next_tx = queue.front()
     - 若 block_size_vb + next_tx > max_block_size: break
     - queue.pop()
     - tx_count += 1
     - block_size_vb += next_tx
     - mempool_current_size_vb -= next_tx

3. 區塊完成:
   - block_count_total += 1

4. 若此區塊屬統計期 (block_count_total > warmup_blocks):

   - stat_block_no = block_count_total - warmup_blocks
   - 紀錄此區塊輸出資料
   - total_completed_transactions += tx_count
   - sum_block_interval += block_interval_sec
   - sum_block_tx_count += tx_count
   - sum_block_size_vb += block_size_vb

5. 重新分配全網算力 (preemption):

- 對所有礦工重新抽 Exp(mean_service_time)
- block_start_time = sim_time
- server_finish_time[i] = sim_time + Exp(mean_service_time)
- time_next_departure = min(server_finish_time)

## 8. 交易大小生成

建議實作:

- 以 GMM + Box-Muller 生成多峰分布
- 強制下限 60 vB

參考函式:

```python
def generate_dynamic_tx_size():
    p = random.random()
    if p < 0.60:
        size = box_muller_normal(mu=141.0, sigma=2.0)
    elif p < 0.85:
        size = box_muller_normal(mu=226.0, sigma=10.0)
    else:
        size = box_muller_normal(mu=370.0, sigma=40.0)
    return max(60, int(round(size)))
```

## 9. 輸出

### 9.1 output.txt (只含統計區塊 1..N)

每行格式:
block_number, block_interval, num_of_tx_in_block, block_size

欄位定義:

- block_number: 統計區塊編號 1..N
- block_interval: 秒
- num_of_tx_in_block: 該區塊交易筆數
- block_size: 輸出 MiB，小數點 2 位

單位定義:
- 內部計算使用 vB
- 對外輸出使用 MiB (2^20)

轉換:

- block_size_mib = block_size_vb / 1048576.0

範例:
1, 600, 1753, 1.06
2, 355, 976, 0.98

### 9.2 mmnl_pow.out

內容:

- 暖機後區塊逐塊資訊
- 總結統計

建議統計公式 (統計期 1..N):

- avg_tx_per_block = sum_block_tx_count / N
- mean_block_interval = sum_block_interval / N
- mean_block_size_mib = (sum_block_size_vb / N) / 1048576.0
- drop_rate_percent = (dropped_transactions / total_arrived_transactions) * 100

安全防呆:

- 若 total_arrived_transactions = 0，drop_rate_percent 定義為 0

## 10. 列印規範

模擬中每個已完成區塊都可印出 (不含暖機):  
#X, Transactions: Y, Interval: Z s, Block Size: W MiB

模擬結束印出統計期摘要:

- Avg. Number of Transactions/Block
- Mean Block Generation Interval
- Mean Block Size
- Total arrived transactions
- Dropped transactions
- Drop rate

## 11. 實作一致性檢查清單

1. 不可使用固定時間步進輪詢，必須使用 next-event。
2. 容量檢查必須是大小相加判斷:
mempool_current_size_vb + tx_size_vb <= queue_capacity
3. 區塊打包必須是前端逐筆累加判斷:
block_size_vb + next_tx_size <= max_block_size
4. 不可拆分交易。
5. 暖機區塊不納入統計。
6. block_count_total 與統計編號 stat_block_no 的換算必須一致。
7. 單位需一致:
   計算使用 vB，輸出 block_size 轉 MiB
8. tie-break 規則固定 (建議 Arrival 優先)。
