import datetime
import requests


def get_block_data(block_height):
    # 偽裝成一般 Chrome 瀏覽器，防止被防火牆或 Cloudflare 阻擋
    headers = {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    }

    try:
        # 步驟 1：取得 Block Hash
        hash_url = f"https://mempool.space/api/block-height/{block_height}"
        hash_response = requests.get(hash_url, headers=headers, timeout=10)

        # 嚴格檢查回傳狀態與內容類型
        if hash_response.status_code != 200:
            print(
                f"無法找到該高度的區塊，錯誤代碼: {hash_response.status_code}"
            )
            return None

        # 如果回傳的內容包含 HTML 標籤，代表被攔截了
        if "<html" in hash_response.text.lower():
            print(
                "連線被阻擋！Mempool.space 回傳了網頁 HTML 而非 API 資料。請嘗試切換網路、關閉 VPN 或使用代理伺服器。"
            )
            return None

        block_hash = hash_response.text.strip()
        print(f"區塊高度 {block_height} 的 Hash 為: {block_hash}")

        # 步驟 2：取得該區塊的詳細 JSON 資料
        block_url = f"https://mempool.space/api/block/{block_hash}"
        block_response = requests.get(block_url, headers=headers, timeout=10)

        if block_response.status_code != 200:
            print(f"無法獲取區塊詳細資料，錯誤代碼: {block_response.status_code}")
            return None

        block_data = block_response.json()

        # 步驟 3：解析所需的欄位
        timestamp = block_data.get("timestamp")
        block_time = datetime.datetime.fromtimestamp(
            timestamp, tz=datetime.timezone.utc
        )
        tx_count = block_data.get("tx_count")
        block_size = block_data.get("size")

        print("\n--- 區塊資料解析成功 ---")
        print(f"區塊產生時間 (UTC): {block_time.strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"包含交易數量: {tx_count} 筆")
        print(f"區塊實際大小: {block_size / 1024 / 1024:.2f} MB")

        return block_data

    except Exception as e:
        print(f"執行發生錯誤: {e}")
        return None


if __name__ == "__main__":
    target_height = 550515  # 您測試的區塊高度
    get_block_data(target_height)
