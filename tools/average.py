import os


def analyze_bitcoin_txt(file_path):
    # 檢查檔案是否存在，防止路徑錯誤崩潰
    if not os.path.exists(file_path):
        print(f"❌ 錯誤：找不到檔案，請確認 {file_path} 存在於指定目錄下。")
        return

    # 初始化儲存列表
    block_intervals = []  # 欄位 2：出塊時間 (秒)
    tx_counts = []  # 欄位 3：交易筆數
    block_sizes_mib = []  # 欄位 4：實體大小 (MiB)

    # 安全地開啟並逐行讀取檔案
    with open(file_path, "r", encoding="utf-8") as file:
        for line_num, line in enumerate(file, 1):
            line = line.strip()
            if not line:  # 跳過空行
                continue

            # 以逗號切割資料並去除前後空格
            parts = [p.strip() for p in line.split(",")]

            # 確保資料格式正確（共 4 欄）
            if len(parts) == 4:
                try:
                    # parts[0] 是區塊編號，忽略它，只取後三欄
                    block_intervals.append(float(parts[1]))
                    tx_counts.append(float(parts[2]))
                    block_sizes_mib.append(float(parts[3]))
                except ValueError:
                    print(f"⚠️ 警告：第 {line_num} 行包含無法轉換為數值的非典型資料，已跳過。")
            else:
                print(f"⚠️ 警告：第 {line_num} 行欄位數量不符合(非4欄)，已跳過。")

    # 檢查是否成功抓取到數據
    total_blocks = len(block_intervals)
    if total_blocks == 0:
        print("❌ 錯誤：檔案內沒有任何合法的統計數據！")
        return

    # 計算統計平均值
    avg_interval = sum(block_intervals) / total_blocks
    avg_tx_count = sum(tx_counts) / total_blocks
    avg_size_mib = sum(block_sizes_mib) / total_blocks

    # 輸出學術報告專用格式
    print("==============================================")
    print(f" 歷史數據檔案: '{file_path}' 讀取解析成功")
    print("==============================================")
    print(f" 總觀測區塊數 : {total_blocks} 個")
    print(f" 出塊間隔時間 (欄位 2) 平均值 : {avg_interval:.2f} 秒")
    print(f" 單區塊交易數量 (欄位 3) 平均值 : {avg_tx_count:.2f} 筆")
    print(f" 單區塊實體大小 (欄位 4) 平均值 : {avg_size_mib:.4f} MiB")
    print("==============================================")


# --- 執行統計 ---
if __name__ == "__main__":
    # 如果您的 txt 檔案放在不同目錄，可以修改為絕對路徑如 r"C:\path\to\your_file.txt"
    target_file = "../outputs/output_real_bitcoin.txt"
    analyze_bitcoin_txt(target_file)
