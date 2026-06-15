from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt


def read_out_file(
	file_path: Path,
) -> tuple[list[int], list[int], list[int], list[float]]:
	"""Read lines like: block_number, block_interval, num_of_tx_in_block, block_size"""
	blocks: list[int] = []
	intervals: list[int] = []
	tx_counts: list[int] = []
	sizes: list[float] = []

	with file_path.open("r", encoding="utf-8") as f:
		for raw_line in f:
			line = raw_line.strip()
			if not line:
				continue

			parts = [p.strip() for p in line.split(",")]
			if len(parts) != 4:
				raise ValueError(f"Invalid format in {file_path}: {line}")

			blocks.append(int(parts[0]))
			intervals.append(int(parts[1]))
			tx_counts.append(int(parts[2]))
			sizes.append(float(parts[3]))

	return blocks, intervals, tx_counts, sizes


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
	ax.tick_params(axis="x", labelsize=9, rotation=45)
	ax.tick_params(axis="y", labelsize=10)
	for label in ax.get_xticklabels():
		label.set_ha("right")

	ax.grid(axis="y", color="#d9d9d9", linewidth=0.8)
	ax.set_axisbelow(True)
	fig.tight_layout()
	fig.savefig(output_path)
	plt.close(fig)


def main() -> None:
	script_dir = Path(__file__).resolve().parent

	parser = argparse.ArgumentParser(
		description="Plot block data from get_block_data output into PNG charts."
	)
	parser.add_argument(
		"--input",
		type=Path,
		default=(script_dir.parent / "output.txt"),
		help="Path to output.txt (default: ../output.txt)",
	)
	parser.add_argument(
		"--output-dir",
		type=Path,
		default=script_dir,
		help="Directory for generated PNG files (default: current script directory)",
	)
	args = parser.parse_args()

	input_file = args.input.resolve()
	output_dir = args.output_dir.resolve()
	output_dir.mkdir(parents=True, exist_ok=True)

	if not input_file.exists():
		raise FileNotFoundError(f"Input file not found: {input_file}")

	blocks, intervals, tx_counts, sizes = read_out_file(input_file)

	charts = [
		(intervals, "Block Generation Interval", "Interval (s)", "block_generation_interval.png"),
		(tx_counts, "Number of Transactions/Block", "Transactions", "num_transactions_block.png"),
		(sizes,     "Block Size",                  "Size (MB)",    "block_size.png"),
	]

	for values, title, y_label, out_name in charts:
		output_path = output_dir / out_name
		plot_bar(blocks, values, title, y_label, output_path)
		print(f"Generated: {output_path}")


if __name__ == "__main__":
	main()