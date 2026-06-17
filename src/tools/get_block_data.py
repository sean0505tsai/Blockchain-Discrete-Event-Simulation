from pathlib import Path
import requests


API_BASE = "https://mempool.space/api"
START_HEIGHT = 550370
END_HEIGHT = 550514
OUTPUT_FILE = Path(__file__).resolve().parents[1] / "output.txt"


def get_headers():
    # 偽裝成一般 Chrome 瀏覽器，防止被防火牆或 Cloudflare 阻擋
    return {
        "User-Agent": (
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/120.0.0.0 Safari/537.36"
        )
    }


def fetch_block_hash(session, block_height):
    url = f"{API_BASE}/block-height/{block_height}"
    response = session.get(url, timeout=10)

    if response.status_code != 200:
        raise RuntimeError(
            f"無法找到高度 {block_height} 的區塊，錯誤代碼: {response.status_code}"
        )

    if "<html" in response.text.lower():
        raise RuntimeError(
            "連線被阻擋！Mempool.space 回傳了 HTML 而非 API 資料。"
        )

    return response.text.strip()


def fetch_block_data(session, block_height):
    block_hash = fetch_block_hash(session, block_height)
    url = f"{API_BASE}/block/{block_hash}"
    response = session.get(url, timeout=10)

    if response.status_code != 200:
        raise RuntimeError(
            f"無法獲取高度 {block_height} 的區塊詳細資料，錯誤代碼: {response.status_code}"
        )

    data = response.json()

    timestamp = data.get("timestamp")
    tx_count = data.get("tx_count")
    block_size_bytes = data.get("size")

    if timestamp is None or tx_count is None or block_size_bytes is None:
        raise RuntimeError(f"高度 {block_height} 的回傳資料缺少必要欄位。")

    return {
        "height": block_height,
        "timestamp": int(timestamp),
        "tx_count": int(tx_count),
        "size_mb": block_size_bytes / 1024 / 1024,
    }


def collect_range_data(start_height, end_height):
    rows = []

    with requests.Session() as session:
        session.headers.update(get_headers())

        # 先抓前一個區塊，才能計算第一個區塊的產生時間
        previous = fetch_block_data(session, start_height - 1)

        for height in range(start_height, end_height + 1):
            current = fetch_block_data(session, height)
            raw_interval = current["timestamp"] - previous["timestamp"]
            block_interval = max(raw_interval, 0)

            if raw_interval < 0:
                print(
                    f"警告：高度 {height} 的時間戳小於前一區塊，已將區塊時間差調整為 0 秒。"
                )

            rows.append(
                {
                    "block_number": current["height"],
                    "block_interval": block_interval,
                    "num_of_tx_in_block": current["tx_count"],
                    "block_size": current["size_mb"],
                }
            )
            previous = current

    return rows

# 輸出格式：block_number, block_interval, num_of_tx_in_block, block_size
def write_output(rows, output_file):
    with output_file.open("w", encoding="utf-8") as f:
        for row in rows:
            line = (
                f"{row['block_number']}, "
                f"{row['block_interval']}, "
                f"{row['num_of_tx_in_block']}, "
                f"{row['block_size']:.2f}"
            )
            f.write(line + "\n")


if __name__ == "__main__":
    try:
        result_rows = collect_range_data(START_HEIGHT, END_HEIGHT)
        write_output(result_rows, OUTPUT_FILE)
        print(f"完成！已輸出 {len(result_rows)} 筆資料到: {OUTPUT_FILE}")
    except Exception as e:
        print(f"執行發生錯誤: {e}")
