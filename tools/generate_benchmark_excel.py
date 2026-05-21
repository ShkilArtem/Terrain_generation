import csv
import math
import sys
from pathlib import Path

try:
    from openpyxl import Workbook
    from openpyxl.chart import BarChart, LineChart, PieChart, Reference, ScatterChart, Series
    from openpyxl.chart.label import DataLabelList
    from openpyxl.worksheet.datavalidation import DataValidation
    from openpyxl.styles import Font, PatternFill
    from openpyxl.utils import get_column_letter
except ModuleNotFoundError:
    codex_site_packages = Path.home() / ".cache" / "codex-runtimes" / "codex-primary-runtime" / "dependencies" / "python" / "Lib" / "site-packages"
    if codex_site_packages.exists():
        sys.path.insert(0, str(codex_site_packages))
        from openpyxl import Workbook
        from openpyxl.chart import BarChart, LineChart, PieChart, Reference, ScatterChart, Series
        from openpyxl.chart.label import DataLabelList
        from openpyxl.worksheet.datavalidation import DataValidation
        from openpyxl.styles import Font, PatternFill
        from openpyxl.utils import get_column_letter
    else:
        raise ModuleNotFoundError(
            "openpyxl is required to generate the Excel workbook. "
            "Install it with: python -m pip install openpyxl"
        )


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BENCHMARK_DIR = PROJECT_ROOT / "benchmark_results"
DEFAULT_CSV_PATH = BENCHMARK_DIR / "terrain_metrics_v2.csv"


def to_number(value):
    try:
        if value == "":
            return None
        return float(value)
    except ValueError:
        return value


def mean(values):
    values = [value for value in values if isinstance(value, (int, float))]
    return sum(values) / len(values) if values else None


def correlation(xs, ys):
    pairs = [(x, y) for x, y in zip(xs, ys) if isinstance(x, (int, float)) and isinstance(y, (int, float))]
    if len(pairs) < 2:
        return None
    xs, ys = zip(*pairs)
    mean_x = sum(xs) / len(xs)
    mean_y = sum(ys) / len(ys)
    numerator = sum((x - mean_x) * (y - mean_y) for x, y in pairs)
    denom_x = math.sqrt(sum((x - mean_x) ** 2 for x in xs))
    denom_y = math.sqrt(sum((y - mean_y) ** 2 for y in ys))
    if denom_x == 0 or denom_y == 0:
        return None
    return numerator / (denom_x * denom_y)


def style_header(ws, row=1):
    fill = PatternFill("solid", fgColor="1F4E79")
    for cell in ws[row]:
        cell.font = Font(bold=True, color="FFFFFF")
        cell.fill = fill


def autofit(ws, max_width=34):
    for col in ws.columns:
        letter = get_column_letter(col[0].column)
        width = min(max_width, max(10, max(len(str(cell.value or "")) for cell in col) + 2))
        ws.column_dimensions[letter].width = width


def append_table(ws, headers, rows, start_row=1, start_col=1):
    for col, header in enumerate(headers, start=start_col):
        ws.cell(start_row, col, header)
    for row_index, row in enumerate(rows, start=start_row + 1):
        for col_index, value in enumerate(row, start=start_col):
            ws.cell(row_index, col_index, value)
    style_header(ws, start_row)
    return start_row, start_row + len(rows)


def nice_major_unit(values, target_ticks=5):
    numeric_values = [abs(value) for value in values if isinstance(value, (int, float))]
    if not numeric_values:
        return None
    max_value = max(numeric_values)
    if max_value <= 0:
        return None
    raw_step = max_value / max(1, target_ticks)
    magnitude = 10 ** math.floor(math.log10(raw_step))
    for multiplier in (1, 2, 5, 10):
        step = multiplier * magnitude
        if raw_step <= step:
            return step
    return 10 * magnitude


def configure_chart(chart, x_title=None, y_title=None, x_format="0", y_format="0.00",
                    x_major_unit=None, y_major_unit=None, legend_position="b"):
    if x_title:
        chart.x_axis.title = x_title
    if y_title:
        chart.y_axis.title = y_title
    chart.x_axis.numFmt = x_format
    chart.y_axis.numFmt = y_format
    chart.x_axis.majorTickMark = "out"
    chart.y_axis.majorTickMark = "out"
    chart.x_axis.tickLblPos = "nextTo"
    chart.y_axis.tickLblPos = "nextTo"
    chart.x_axis.majorGridlines = None
    if x_major_unit:
        chart.x_axis.majorUnit = x_major_unit
    if y_major_unit:
        chart.y_axis.majorUnit = y_major_unit
    if chart.legend:
        chart.legend.position = legend_position


def add_line_chart(ws, title, data_range, category_range, anchor, x_title=None, y_title=None,
                   x_format="0", y_format="0.00", y_major_unit=None):
    chart = LineChart()
    chart.title = title
    chart.add_data(data_range, titles_from_data=True)
    chart.set_categories(category_range)
    chart.height = 10
    chart.width = 20
    configure_chart(chart, x_title, y_title, x_format=x_format, y_format=y_format, y_major_unit=y_major_unit)
    ws.add_chart(chart, anchor)
    return chart


def add_scatter_chart(ws, title, x_ref, y_ref, anchor, x_title, y_title,
                      x_format="0", y_format="0.00", x_major_unit=None, y_major_unit=None,
                      series_title=None):
    chart = ScatterChart()
    chart.title = title
    chart.series.append(Series(y_ref, x_ref, title=series_title or y_title))
    chart.height = 10
    chart.width = 20
    configure_chart(chart, x_title, y_title, x_format=x_format, y_format=y_format,
                    x_major_unit=x_major_unit, y_major_unit=y_major_unit)
    ws.add_chart(chart, anchor)
    return chart


def main():
    csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CSV_PATH
    if not csv_path.is_absolute():
        csv_path = PROJECT_ROOT / csv_path
    if not csv_path.exists():
        available_csvs = sorted(BENCHMARK_DIR.glob("*.csv")) if BENCHMARK_DIR.exists() else []
        available_text = "\n".join(f"  - {path.relative_to(PROJECT_ROOT)}" for path in available_csvs)
        if not available_text:
            available_text = "  (no CSV files found)"
        raise FileNotFoundError(
            f"Missing benchmark CSV: {csv_path}\n\n"
            "Run the benchmark inside the app first:\n"
            "  1. Open the ImGui window: Geological Analysis & Parameter Sweep\n"
            "  2. Click: Execute Automated Parameter Sweep\n"
            "  3. Then rerun this command.\n\n"
            "Available CSV files in benchmark_results:\n"
            f"{available_text}"
        )

    BENCHMARK_DIR.mkdir(exist_ok=True)
    xlsx_path = BENCHMARK_DIR / f"{csv_path.stem}_analysis.xlsx"

    with csv_path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = [{key: to_number(value) for key, value in row.items()} for row in reader]
    if not rows:
        raise ValueError("CSV has a header but no benchmark rows yet.")

    for i, row in enumerate(rows, start=1):
        row.setdefault("Run", i)
        row.setdefault("IncludeInCharts", 1)
        row.setdefault("ExperimentalValue", row.get("Iterations"))
        row.setdefault("ExperimentalParameter", "Benchmark")
        row.setdefault("MemoryEstimate_MB", estimate_memory_mb(row.get("GridSize") or 0))

    headers = list(rows[0].keys())
    bin_headers = [f"Bin{i}" for i in range(90)]
    hypso_headers = [f"HypsoAbove{i}" for i in range(101)]
    derived_headers = [
        "Run",
        "IncludeInCharts",
        "ExperimentalParameter",
        "ExperimentalValue",
        "GridSize",
        "Iterations",
        "GenTime_ms",
        "ErosionTime_ms",
        "ThermalTime_ms",
        "PhysicsTime_ms",
        "TotalTime_ms",
        "GenTime_pct",
        "ErosionTime_pct",
        "ThermalTime_pct",
        "MemoryEstimate_MB",
        "TotalVolumeMoved",
        "MeanSlope_deg",
        "P95Slope_deg",
        "HypsometricAUC",
    ]
    derived_rows = [derive_row(row, bin_headers, hypso_headers) for row in rows]
    active_derived_rows = [row for row in derived_rows if row.get("IncludeInCharts", 1) != 0] or derived_rows
    latest_derived = active_derived_rows[-1]
    latest_run = latest_derived.get("Run")
    latest = next((row for row in rows if row.get("Run") == latest_run), rows[-1])

    wb = Workbook()
    ws_readme = wb.active
    ws_readme.title = "README"
    ws_raw = wb.create_sheet("Raw Data")
    ws_derived = wb.create_sheet("Derived Metrics")
    ws_selector = wb.create_sheet("Run Selector")
    ws_manager = wb.create_sheet("Run Manager")
    ws_grid = wb.create_sheet("Cost Grid Size")
    ws_iter = wb.create_sheet("Cost Iterations")
    ws_profile = wb.create_sheet("Profiling")
    ws_realism = wb.create_sheet("Realism Metrics")
    ws_tradeoff = wb.create_sheet("Tradeoff Elbow")
    ws_corr = wb.create_sheet("Correlations")

    write_readme(ws_readme)

    raw_headers = headers
    append_table(ws_raw, raw_headers, [[row.get(header) for header in raw_headers] for row in rows])
    ws_raw.freeze_panes = "A2"

    append_table(ws_derived, derived_headers, [[row.get(header) for header in derived_headers] for row in derived_rows])
    ws_derived.freeze_panes = "A2"

    write_run_selector_sheet(ws_selector, rows, derived_rows, headers, derived_headers, bin_headers, hypso_headers)
    write_run_manager_sheet(ws_manager, derived_rows)
    write_cost_grid_sheet(ws_grid, active_derived_rows)
    write_cost_iterations_sheet(ws_iter, active_derived_rows)
    write_profiling_sheet(ws_profile, latest_derived)
    write_realism_sheet(ws_realism, active_derived_rows, bin_headers, hypso_headers)
    write_tradeoff_sheet(ws_tradeoff, active_derived_rows, derived_headers)
    write_correlations_sheet(ws_corr, active_derived_rows)

    for ws in wb.worksheets:
        ws.sheet_view.showGridLines = False
        autofit(ws)

    wb.save(xlsx_path)
    print(f"Saved {xlsx_path}")


def estimate_memory_mb(grid_size):
    try:
        n = int(grid_size)
    except (TypeError, ValueError):
        return None
    vertex_bytes = n * n * 16 * 4
    index_bytes = max(0, n - 1) * max(0, n - 1) * 6 * 4
    working_bytes = n * n * 4
    return (vertex_bytes + index_bytes + working_bytes) / (1024 * 1024)


def derive_row(row, bin_headers, hypso_headers):
    bins = [row.get(header, 0) or 0 for header in bin_headers]
    total_bins = sum(bins) or 1
    mean_slope = sum(count * (degree + 0.5) for degree, count in enumerate(bins)) / total_bins
    cumulative = 0
    p95_slope = 89
    for degree, count in enumerate(bins):
        cumulative += count
        if cumulative / total_bins >= 0.95:
            p95_slope = degree
            break
    hypso_values = [row.get(header, 0) or 0 for header in hypso_headers]
    hypso_auc = sum(hypso_values) / len(hypso_values)
    gen = row.get("GenTime_ms") or 0
    erosion = row.get("ErosionTime_ms") or 0
    thermal = row.get("ThermalTime_ms") or 0
    physics_time = erosion + thermal
    total_time = gen + erosion + thermal
    return {
        "Run": row.get("Run"),
        "IncludeInCharts": row.get("IncludeInCharts", 1),
        "ExperimentalParameter": row.get("ExperimentalParameter", "Benchmark"),
        "ExperimentalValue": row.get("ExperimentalValue"),
        "GridSize": row.get("GridSize"),
        "Iterations": row.get("Iterations"),
        "GenTime_ms": gen,
        "ErosionTime_ms": erosion,
        "ThermalTime_ms": thermal,
        "PhysicsTime_ms": physics_time,
        "TotalTime_ms": total_time,
        "GenTime_pct": 100 * gen / total_time if total_time else 0,
        "ErosionTime_pct": 100 * erosion / total_time if total_time else 0,
        "ThermalTime_pct": 100 * thermal / total_time if total_time else 0,
        "MemoryEstimate_MB": row.get("MemoryEstimate_MB") or estimate_memory_mb(row.get("GridSize")),
        "TotalVolumeMoved": row.get("TotalVolumeMoved"),
        "MeanSlope_deg": mean_slope,
        "P95Slope_deg": p95_slope,
        "HypsometricAUC": hypso_auc,
    }


def write_readme(ws):
    rows = [
        ["Sheet", "Purpose"],
        ["Raw Data", "Direct CSV import. One benchmark or sweep step equals one row."],
        ["Derived Metrics", "Adds total runtime (ms), physics runtime (ms), stage percentages (%), mean slope (deg), P95 slope (deg), hypsometric AUC (%), and memory estimate (MB)."],
        ["Run Selector", "Interactive single-run view. Choose a Run number in B2 to switch profiling, slope histogram, and hypsometric charts."],
        ["Run Manager", "Interactive run filtering. Set Include in filtered charts to 0 to hide a benchmark run from the manager charts without deleting raw data."],
        ["Cost Grid Size", "Use for Coût de calcul: time (ms) and memory (MB) versus fixed GRID_SIZE (vertices per side)."],
        ["Cost Iterations", "Use for Coût de calcul: runtime (ms) versus droplet count K. Expected hydraulic erosion trend is O(K)."],
        ["Profiling", "Pie chart of generation, hydraulic erosion, and thermal weathering time share (% of total run time)."],
        ["Realism Metrics", "Selected-run realism view. Charts follow the Run Selector dropdown: slope histogram and hypsometric curve for the chosen run."],
        ["Tradeoff Elbow", "Two paired views: averaged curves across active runs, plus selected-run point charts linked to Run Selector."],
        ["Correlations", "Pearson correlation matrix between runtime, memory, volume moved, and morphology metrics."],
        [],
        ["How to run", "1. Run benchmarks/sweeps in the app. 2. Run: python tools/generate_benchmark_excel.py benchmark_results/parameter_sweep_report_v2.csv"],
    ]
    for row in rows:
        ws.append(row)
    style_header(ws)
    ws["A1"].font = Font(bold=True, color="FFFFFF")


def column_letter(headers, header):
    try:
        return get_column_letter(headers.index(header) + 1)
    except ValueError:
        return None


def index_formula(sheet, value_col_letter, match_col_letter, match_cell):
    return f"=INDEX('{sheet}'!${value_col_letter}:${value_col_letter},MATCH({match_cell},'{sheet}'!${match_col_letter}:${match_col_letter},0))"


def write_run_selector_sheet(ws, rows, derived_rows, raw_headers, derived_headers, bin_headers, hypso_headers):
    ws["A1"] = "Selected Benchmark Run"
    ws["A2"] = "Choose run:"
    ws["B2"] = derived_rows[-1].get("Run") if derived_rows else 1
    style_header(ws, 1)

    run_list_col = 26
    run_list_letter = get_column_letter(run_list_col)
    ws.cell(1, run_list_col, "Available runs")
    for i, row in enumerate(derived_rows, start=2):
        ws.cell(i, run_list_col, row.get("Run"))
    ws.column_dimensions[run_list_letter].hidden = True
    validation = DataValidation(type="list", formula1=f"=${run_list_letter}$2:${run_list_letter}${len(derived_rows) + 1}", allow_blank=False)
    ws.add_data_validation(validation)
    validation.add(ws["B2"])

    raw_run_col = column_letter(raw_headers, "Run")
    derived_run_col = column_letter(derived_headers, "Run")
    derived_cols = {header: column_letter(derived_headers, header) for header in derived_headers}

    summary_rows = [
        ("Experimental parameter", "ExperimentalParameter"),
        ("Experimental value", "ExperimentalValue"),
        ("Grid size (vertices/side)", "GridSize"),
        ("Droplet iterations (count)", "Iterations"),
        ("fBm generation (ms)", "GenTime_ms"),
        ("Hydraulic erosion (ms)", "ErosionTime_ms"),
        ("Thermal weathering (ms)", "ThermalTime_ms"),
        ("Physics simulation time (ms)", "PhysicsTime_ms"),
        ("Total runtime incl. generation (ms)", "TotalTime_ms"),
        ("Moved volume (height units)", "TotalVolumeMoved"),
        ("Mean slope (deg)", "MeanSlope_deg"),
        ("P95 slope (deg)", "P95Slope_deg"),
        ("Hypsometric AUC (%)", "HypsometricAUC"),
    ]
    ws["A4"] = "Metric"
    ws["B4"] = "Selected run value"
    style_header(ws, 4)
    for i, (label, key) in enumerate(summary_rows, start=5):
        ws.cell(i, 1, label)
        col = derived_cols.get(key)
        ws.cell(i, 2, index_formula("Derived Metrics", col, derived_run_col, "$B$2") if col and derived_run_col else None)

    ws["D4"] = "Stage"
    ws["E4"] = "Time (ms)"
    ws["F4"] = "Share of total time (%)"
    style_header(ws, 4)
    profile_rows = [
        ("fBm noise generation", "GenTime_ms", "GenTime_pct"),
        ("Hydraulic erosion", "ErosionTime_ms", "ErosionTime_pct"),
        ("Thermal weathering", "ThermalTime_ms", "ThermalTime_pct"),
    ]
    for i, (stage, time_key, pct_key) in enumerate(profile_rows, start=5):
        ws.cell(i, 4, stage)
        ws.cell(i, 5, index_formula("Derived Metrics", derived_cols.get(time_key), derived_run_col, "$B$2"))
        ws.cell(i, 6, index_formula("Derived Metrics", derived_cols.get(pct_key), derived_run_col, "$B$2"))

    ws["A20"] = "Slope angle (degrees)"
    ws["B20"] = "Vertex count (cells)"
    ws["D20"] = "Normalized height threshold (%)"
    ws["E20"] = "Area above threshold (%)"
    style_header(ws, 20)

    for degree, header in enumerate(bin_headers, start=21):
        ws.cell(degree, 1, degree - 21)
        raw_col = column_letter(raw_headers, header)
        ws.cell(degree, 2, index_formula("Raw Data", raw_col, raw_run_col, "$B$2") if raw_col and raw_run_col else 0)
    for i, header in enumerate(hypso_headers, start=21):
        ws.cell(i, 4, i - 21)
        raw_col = column_letter(raw_headers, header)
        ws.cell(i, 5, index_formula("Raw Data", raw_col, raw_run_col, "$B$2") if raw_col and raw_run_col else 0)

    profile_chart = BarChart()
    profile_chart.title = "Selected Run: Stage Runtime"
    profile_chart.add_data(Reference(ws, min_col=5, min_row=4, max_row=7), titles_from_data=True)
    profile_chart.set_categories(Reference(ws, min_col=4, min_row=5, max_row=7))
    configure_chart(profile_chart, x_title="Simulation stage", y_title="Execution time (ms)", y_format="0")
    profile_chart.height = 9
    profile_chart.width = 16
    ws.add_chart(profile_chart, "H2")

    slope_chart = BarChart()
    slope_chart.title = "Selected Run: Slope Histogram"
    slope_chart.add_data(Reference(ws, min_col=2, min_row=20, max_row=110), titles_from_data=True)
    slope_chart.set_categories(Reference(ws, min_col=1, min_row=21, max_row=110))
    configure_chart(slope_chart, x_title="Slope angle (degrees)", y_title="Vertex count (cells)", y_format="0")
    slope_chart.height = 10
    slope_chart.width = 18
    ws.add_chart(slope_chart, "H20")

    hypso_chart = LineChart()
    hypso_chart.title = "Selected Run: Hypsometric Curve"
    hypso_chart.add_data(Reference(ws, min_col=5, min_row=20, max_row=121), titles_from_data=True)
    hypso_chart.set_categories(Reference(ws, min_col=4, min_row=21, max_row=121))
    configure_chart(
        hypso_chart,
        x_title="Normalized height threshold (%)",
        y_title="Area above threshold (%)",
        x_format="0",
        y_format="0",
        x_major_unit=10,
        y_major_unit=10,
    )
    hypso_chart.height = 10
    hypso_chart.width = 18
    ws.add_chart(hypso_chart, "H38")


def write_run_manager_sheet(ws, derived_rows):
    ws["A1"] = "Run Manager"
    ws["A2"] = "Set Include in filtered charts to 0 to hide a run from the charts on this sheet. Raw CSV data is not physically deleted."
    style_header(ws, 1)

    headers = [
        "Run",
        "Include in filtered charts (1 keep / 0 hide)",
        "Experimental parameter",
        "Experimental value",
        "Grid size (vertices/side)",
        "Droplet iterations (count)",
        "Physics simulation time (ms)",
        "Moved volume (height units)",
    ]
    rows = [
        [
            row.get("Run"),
            row.get("IncludeInCharts", 1),
            row.get("ExperimentalParameter"),
            row.get("ExperimentalValue"),
            row.get("GridSize"),
            row.get("Iterations"),
            row.get("PhysicsTime_ms"),
            row.get("TotalVolumeMoved"),
        ]
        for row in derived_rows
    ]
    start, end = append_table(ws, headers, rows, start_row=4, start_col=1)
    validation = DataValidation(type="list", formula1='"0,1"', allow_blank=False)
    ws.add_data_validation(validation)
    validation.add(f"B{start + 1}:B{end}")

    helper_headers = ["Filtered iterations", "Filtered physics time (ms)", "Filtered moved volume"]
    for col, header in enumerate(helper_headers, start=10):
        ws.cell(start, col, header)
    style_header(ws, start)
    for row in range(start + 1, end + 1):
        ws.cell(row, 10, f'=IF($B{row}=1,$F{row},NA())')
        ws.cell(row, 11, f'=IF($B{row}=1,$G{row},NA())')
        ws.cell(row, 12, f'=IF($B{row}=1,$H{row},NA())')

    if rows:
        iterations = [row[5] for row in rows]
        physics = [row[6] for row in rows]
        volume = [row[7] for row in rows]
        add_scatter_chart(
            ws,
            "Filtered Runs: Iterations vs Volume Moved",
            Reference(ws, min_col=10, min_row=start + 1, max_row=end),
            Reference(ws, min_col=12, min_row=start + 1, max_row=end),
            "N4",
            "Droplet iterations (count)",
            "Moved volume (height units)",
            x_major_unit=nice_major_unit(iterations),
            y_major_unit=nice_major_unit(volume),
            series_title="Included runs only",
        )
        add_scatter_chart(
            ws,
            "Filtered Runs: Cost vs Realism",
            Reference(ws, min_col=11, min_row=start + 1, max_row=end),
            Reference(ws, min_col=12, min_row=start + 1, max_row=end),
            "N22",
            "Physics Simulation Time (ms)",
            "Moved volume (height units)",
            x_major_unit=nice_major_unit(physics),
            y_major_unit=nice_major_unit(volume),
            series_title="Included runs only",
        )


def grouped_average(rows, key):
    groups = {}
    for row in rows:
        value = row.get(key)
        if value is None:
            continue
        groups.setdefault(value, []).append(row)
    output = []
    for value in sorted(groups):
        group = groups[value]
        output.append([
            value,
            mean([r.get("GenTime_ms") for r in group]),
            mean([r.get("ErosionTime_ms") for r in group]),
            mean([r.get("ThermalTime_ms") for r in group]),
            mean([r.get("TotalTime_ms") for r in group]),
            mean([r.get("MemoryEstimate_MB") for r in group]),
            mean([r.get("TotalVolumeMoved") for r in group]),
        ])
    return output


def write_cost_grid_sheet(ws, rows):
    grid_rows = [r for r in rows if r.get("ExperimentalParameter") == "Grid Size"] or rows
    table = grouped_average(grid_rows, "GridSize")
    headers = ["GRID_SIZE (vertices/side)", "fBm generation (ms)", "Hydraulic erosion (ms)", "Thermal weathering (ms)", "Total runtime (ms)", "Estimated memory (MB)", "Moved volume (height units)"]
    start, end = append_table(ws, headers, table)
    if table:
        time_values = [value for row in table for value in row[1:5]]
        memory_values = [row[5] for row in table]
        add_line_chart(
            ws,
            "Time vs GRID_SIZE",
            Reference(ws, min_col=2, max_col=5, min_row=start, max_row=end),
            Reference(ws, min_col=1, min_row=start + 1, max_row=end),
            "I2",
            x_title="GRID_SIZE (vertices per side)",
            y_title="Execution time (ms)",
            x_format="0",
            y_format="0",
            y_major_unit=nice_major_unit(time_values),
        )
        add_line_chart(
            ws,
            "Estimated Memory vs GRID_SIZE",
            Reference(ws, min_col=6, min_row=start, max_row=end),
            Reference(ws, min_col=1, min_row=start + 1, max_row=end),
            "I20",
            x_title="GRID_SIZE (vertices per side)",
            y_title="Estimated memory (MB)",
            x_format="0",
            y_format="0.0",
            y_major_unit=nice_major_unit(memory_values),
        )


def write_cost_iterations_sheet(ws, rows):
    iteration_rows = [r for r in rows if r.get("ExperimentalParameter") == "Iteration Count"] or rows
    table = grouped_average(iteration_rows, "Iterations")
    headers = ["Droplet iterations (count)", "fBm generation (ms)", "Hydraulic erosion (ms)", "Thermal weathering (ms)", "Total runtime (ms)", "Estimated memory (MB)", "Moved volume (height units)"]
    start, end = append_table(ws, headers, table)
    if table:
        time_values = [value for row in table for value in row[2:5]]
        iteration_values = [row[0] for row in table]
        volume_values = [row[6] for row in table]
        add_line_chart(
            ws,
            "Time vs Droplet Iterations",
            Reference(ws, min_col=3, max_col=5, min_row=start, max_row=end),
            Reference(ws, min_col=1, min_row=start + 1, max_row=end),
            "I2",
            x_title="Droplet iterations (count)",
            y_title="Execution time (ms)",
            x_format="0",
            y_format="0",
            y_major_unit=nice_major_unit(time_values),
        )
        add_scatter_chart(
            ws,
            "Iterations vs Volume Moved",
            Reference(ws, min_col=1, min_row=start + 1, max_row=end),
            Reference(ws, min_col=7, min_row=start + 1, max_row=end),
            "I20",
            "Droplet iterations (count)",
            "Moved volume (height units)",
            x_format="0",
            y_format="0.00",
            x_major_unit=nice_major_unit(iteration_values),
            y_major_unit=nice_major_unit(volume_values),
            series_title="Moved volume (height units)",
        )


def write_profiling_sheet(ws, latest):
    rows = [
        ["Stage", "Time (ms)", "Share of total time (%)"],
        ["fBm noise generation", latest.get("GenTime_ms"), latest.get("GenTime_pct")],
        ["Hydraulic erosion", latest.get("ErosionTime_ms"), latest.get("ErosionTime_pct")],
        ["Thermal weathering", latest.get("ThermalTime_ms"), latest.get("ThermalTime_pct")],
    ]
    for row in rows:
        ws.append(row)
    style_header(ws)
    pie = PieChart()
    pie.title = "Profiling: Time Share by Stage"
    pie.add_data(Reference(ws, min_col=2, min_row=1, max_row=4), titles_from_data=True)
    pie.set_categories(Reference(ws, min_col=1, min_row=2, max_row=4))
    pie.legend.position = "r"
    pie.dataLabels = DataLabelList()
    pie.dataLabels.showPercent = True
    pie.dataLabels.showLeaderLines = True
    pie.height = 12
    pie.width = 16
    ws.add_chart(pie, "E2")


def write_realism_sheet(ws, derived_rows, bin_headers, hypso_headers):
    ws["A1"] = "Selected Run: Slope Histogram"
    ws["D1"] = "Selected Run: Hypsometric Curve"
    ws["G1"] = "Selected Run Summary"
    ws["G2"] = "This sheet follows the Run Selector dropdown."
    ws["A2"] = "Slope angle (degrees)"
    ws["B2"] = "Vertex count (cells)"
    for degree, header in enumerate(bin_headers, start=3):
        ws.cell(degree, 1, degree - 3)
        ws.cell(degree, 2, f"='Run Selector'!B{degree + 18}")
    ws["D2"] = "Normalized height threshold (%)"
    ws["E2"] = "Area above threshold (%)"
    for i, header in enumerate(hypso_headers, start=3):
        ws.cell(i, 4, i - 3)
        ws.cell(i, 5, f"='Run Selector'!E{i + 18}")
    summary_rows = [
        ("Selected run", "='Run Selector'!$B$2"),
        ("Experimental parameter", "='Run Selector'!$B$5"),
        ("Experimental value", "='Run Selector'!$B$6"),
        ("Grid size (vertices/side)", "='Run Selector'!$B$7"),
        ("Droplet iterations (count)", "='Run Selector'!$B$8"),
        ("Moved volume (height units)", "='Run Selector'!$B$14"),
        ("Mean slope (deg)", "='Run Selector'!$B$15"),
        ("P95 slope (deg)", "='Run Selector'!$B$16"),
        ("Hypsometric AUC (%)", "='Run Selector'!$B$17"),
    ]
    append_table(ws, ["Metric", "Selected run value"], summary_rows, start_row=3, start_col=7)
    for row in [1, 2]:
        style_header(ws, row)
    slope_chart = BarChart()
    slope_chart.title = "Selected Run: Slope Histogram"
    slope_chart.add_data(Reference(ws, min_col=2, min_row=2, max_row=92), titles_from_data=True)
    slope_chart.set_categories(Reference(ws, min_col=1, min_row=3, max_row=92))
    configure_chart(
        slope_chart,
        x_title="Slope angle (degrees)",
        y_title="Vertex count (cells)",
        x_format="0",
        y_format="0",
    )
    slope_chart.height = 10
    slope_chart.width = 18
    ws.add_chart(slope_chart, "M2")
    hypso_chart = LineChart()
    hypso_chart.title = "Selected Run: Hypsometric Curve"
    hypso_chart.add_data(Reference(ws, min_col=5, min_row=2, max_row=103), titles_from_data=True)
    hypso_chart.set_categories(Reference(ws, min_col=4, min_row=3, max_row=103))
    configure_chart(
        hypso_chart,
        x_title="Normalized height threshold (%)",
        y_title="Area above threshold (%)",
        x_format="0",
        y_format="0",
        x_major_unit=10,
        y_major_unit=10,
    )
    hypso_chart.height = 10
    hypso_chart.width = 18
    ws.add_chart(hypso_chart, "M20")


def grouped_tradeoff_average(rows):
    groups = {}
    for row in rows:
        key = row.get("Iterations")
        if key is None:
            continue
        groups.setdefault(key, []).append(row)
    output = []
    for iterations in sorted(groups):
        group = groups[iterations]
        output.append([
            iterations,
            mean([row.get("PhysicsTime_ms") for row in group]),
            mean([row.get("TotalTime_ms") for row in group]),
            mean([row.get("TotalVolumeMoved") for row in group]),
            mean([row.get("HypsometricAUC") for row in group]),
            mean([row.get("MeanSlope_deg") for row in group]),
            mean([row.get("P95Slope_deg") for row in group]),
            len(group),
        ])
    return output


def write_tradeoff_sheet(ws, rows, derived_headers):
    average_headers = [
        "Droplet iterations (count)",
        "Average physics simulation time (ms)",
        "Average total runtime incl. generation (ms)",
        "Average moved volume (height units)",
        "Average hypsometric AUC (%)",
        "Average mean slope (deg)",
        "Average P95 slope (deg)",
        "Runs averaged (count)",
    ]
    average_table = grouped_tradeoff_average(rows)
    avg_start, avg_end = append_table(ws, average_headers, average_table, start_row=1, start_col=1)

    selected_start = max(avg_end + 3, 14)
    selected_headers = [
        "Selected run",
        "Droplet iterations (count)",
        "Physics simulation time (ms)",
        "Total runtime incl. generation (ms)",
        "Moved volume (height units)",
        "Hypsometric AUC (%)",
        "Mean slope (deg)",
        "P95 slope (deg)",
    ]
    append_table(ws, selected_headers, [[]], start_row=selected_start, start_col=1)
    ws.cell(selected_start + 1, 1, "='Run Selector'!$B$2")
    selected_keys = ["Iterations", "PhysicsTime_ms", "TotalTime_ms", "TotalVolumeMoved", "HypsometricAUC", "MeanSlope_deg", "P95Slope_deg"]
    run_col = column_letter(derived_headers, "Run")
    for offset, key in enumerate(selected_keys, start=2):
        col = column_letter(derived_headers, key)
        ws.cell(selected_start + 1, offset, index_formula("Derived Metrics", col, run_col, "$A$" + str(selected_start + 1)))

    if average_table:
        iteration_values = [row[0] for row in average_table]
        physics_values = [row[1] for row in average_table]
        volume_values = [row[3] for row in average_table]
        add_scatter_chart(
            ws,
            "Average Elbow Curve: Iterations vs Realism",
            Reference(ws, min_col=1, min_row=avg_start + 1, max_row=avg_end),
            Reference(ws, min_col=4, min_row=avg_start + 1, max_row=avg_end),
            "J2",
            "Droplet iterations (count)",
            "Average moved volume (height units)",
            x_format="0",
            y_format="0.00",
            x_major_unit=nice_major_unit(iteration_values),
            y_major_unit=nice_major_unit(volume_values),
            series_title="Average moved volume",
        )
        add_scatter_chart(
            ws,
            "Average Cost vs Realism",
            Reference(ws, min_col=2, min_row=avg_start + 1, max_row=avg_end),
            Reference(ws, min_col=4, min_row=avg_start + 1, max_row=avg_end),
            "J20",
            "Average physics simulation time (ms)",
            "Average moved volume (height units)",
            x_format="0",
            y_format="0.00",
            x_major_unit=nice_major_unit(physics_values),
            y_major_unit=nice_major_unit(volume_values),
            series_title="Average moved volume",
        )

        add_scatter_chart(
            ws,
            "Selected Run: Iterations vs Realism",
            Reference(ws, min_col=2, min_row=selected_start + 1, max_row=selected_start + 1),
            Reference(ws, min_col=5, min_row=selected_start + 1, max_row=selected_start + 1),
            "J38",
            "Selected run droplet iterations (count)",
            "Selected run moved volume (height units)",
            x_format="0",
            y_format="0.00",
            x_major_unit=nice_major_unit(iteration_values),
            y_major_unit=nice_major_unit(volume_values),
            series_title="Selected run moved volume",
        )
        add_scatter_chart(
            ws,
            "Selected Run: Cost vs Realism",
            Reference(ws, min_col=3, min_row=selected_start + 1, max_row=selected_start + 1),
            Reference(ws, min_col=5, min_row=selected_start + 1, max_row=selected_start + 1),
            "J56",
            "Selected run physics simulation time (ms)",
            "Selected run moved volume (height units)",
            x_format="0",
            y_format="0.00",
            x_major_unit=nice_major_unit(physics_values),
            y_major_unit=nice_major_unit(volume_values),
            series_title="Selected run moved volume",
        )


def write_correlations_sheet(ws, rows):
    metric_names = [
        "ExperimentalValue",
        "GridSize",
        "Iterations",
        "GenTime_ms",
        "ErosionTime_ms",
        "ThermalTime_ms",
        "PhysicsTime_ms",
        "TotalTime_ms",
        "MemoryEstimate_MB",
        "TotalVolumeMoved",
        "MeanSlope_deg",
        "P95Slope_deg",
        "HypsometricAUC",
    ]
    ws.append(["Metric", *metric_names])
    for metric_a in metric_names:
        ws.append([
            metric_a,
            *[
                correlation([r.get(metric_a) for r in rows], [r.get(metric_b) for r in rows])
                for metric_b in metric_names
            ],
        ])
    style_header(ws)
    for row in ws.iter_rows(min_row=2, min_col=2):
        for cell in row:
            cell.number_format = "0.00"


if __name__ == "__main__":
    main()
