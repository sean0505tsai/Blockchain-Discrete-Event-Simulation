from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt


def read_out_file(file_path: Path) -> tuple[list[int], list[float]]:
	"""Read lines like: block_number, value"""
	blocks: list[int] = []
	values: list[float] = []

	with file_path.open("r", encoding="utf-8") as f:
		for raw_line in f:
			line = raw_line.strip()
			if not line:
				continue

			parts = [p.strip() for p in line.split(",")]
			if len(parts) != 2:
				raise ValueError(f"Invalid format in {file_path}: {line}")

			block = int(parts[0])
			value = float(parts[1])
			blocks.append(block)
			values.append(value)

	return blocks, values


def make_ticks(blocks: list[int], count: int = 12) -> list[int]:
	if not blocks:
		return []

	n = len(blocks)
	if n <= count:
		return blocks

	step = max(1, n // (count - 1))
	ticks = [blocks[i] for i in range(0, n, step)]
	if ticks[-1] != blocks[-1]:
		ticks.append(blocks[-1])
	return ticks


def plot_bar(
	blocks: list[int],
	values: list[float],
	title: str,
	y_label: str,
	output_path: Path,
) -> None:
	fig, ax = plt.subplots(figsize=(12, 6), dpi=120)
	ax.set_facecolor("#f2f2f2")
	ax.bar(blocks, values, color="#222222", edgecolor="#222222", width=0.35)

	ax.set_title(title, fontsize=18, pad=12)
	ax.set_xlabel("Blocks", fontsize=12)
	ax.set_ylabel(y_label, fontsize=12)

	ticks = make_ticks(blocks, count=12)
	ax.set_xticks(ticks)
	ax.tick_params(axis="x", labelsize=9)
	ax.tick_params(axis="y", labelsize=10)

	ax.grid(axis="y", color="#d9d9d9", linewidth=0.8)
	ax.set_axisbelow(True)
	fig.tight_layout()
	fig.savefig(output_path)
	plt.close(fig)


def main() -> None:
	script_dir = Path(__file__).resolve().parent

	parser = argparse.ArgumentParser(
		description="Plot Plan A simulation outputs into PNG charts."
	)
	parser.add_argument(
		"--input-dir",
		type=Path,
		default=(script_dir.parent / "plan_A"),
		help="Directory containing *.out files (default: ../plan_A)",
	)
	parser.add_argument(
		"--output-dir",
		type=Path,
		default=script_dir,
		help="Directory for generated PNG files (default: current script directory)",
	)
	args = parser.parse_args()

	input_dir = args.input_dir.resolve()
	output_dir = args.output_dir.resolve()
	output_dir.mkdir(parents=True, exist_ok=True)

	files_to_plot = [
		(
			"num_transactions_block.out",
			"Number of Transactions/Block",
			"Transactions",
			"num_transactions_block.png",
		),
		(
			"block_generation_interval.out",
			"Block Generation Interval",
			"Interval (s)",
			"block_generation_interval.png",
		),
		(
			"utilization.out",
			"Utilization of Servers",
			"Utilization (%)",
			"utilization.png",
		),
	]

	for file_name, title, y_label, out_name in files_to_plot:
		file_path = input_dir / file_name
		if not file_path.exists():
			raise FileNotFoundError(f"Input file not found: {file_path}")

		blocks, values = read_out_file(file_path)
		
		# --- 新增的轉換邏輯開始 ---
		# 如果處理的檔案是區塊生成間隔，則將數值從秒除以 60 轉換為分鐘，並修改 Y 軸標籤
		if file_name == "block_generation_interval.out":
			values = [v / 60.0 for v in values]
			y_label = "Interval (min)"
		# --- 新增的轉換邏輯結束 ---

		output_path = output_dir / out_name
		plot_bar(blocks, values, title, y_label, output_path)
		print(f"Generated: {output_path}")


if __name__ == "__main__":
	main()