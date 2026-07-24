import os
import re
import glob
import argparse
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
from matplotlib.patches import Patch

# ===== FONT AND STYLE SETTINGS =====
FONT_SIZE = 10
LABEL_SIZE = 12
TICK_SIZE = 12
LEGEND_SIZE = 12
ANNOTATION_SIZE = 12
SPEEDUP_SIZE = 10

SINGLE_COLUMN_FIGURE_SIZE = (10, 5)
SINGLE_COLUMN_FONT_SIZES = {
    'label': 18,
    'tick': 16,
    'legend': 15,
    'annotation': 16,
}
PERFORMANCE_FONT_SIZES = {
    'label': 20,
    'tick': 18,
    'legend': 14,
    'annotation': 18,
}
PERFORMANCE_FIGURE_SIZE = SINGLE_COLUMN_FIGURE_SIZE
E2E_COMBINED_FIGURE_SIZE = (48, 7)
E2E_COMBINED_FONT_SIZES = {
    'label': 40,
    'tick': 36,
    'legend': 34,
    'annotation': 36,
}
ABLATION_COMBINED_FONT_SIZES = {
    'label': 40,
    'tick': 36,
    'legend': 34,
    'annotation': 36,
}
MOTIVATION_COMBINED_FONT_SIZES = {
    'label': 40,
    'tick': 36,
    'legend': 34,
    'annotation': 36,
}
E2E_DIRECTION_Y = 0.88
PERFORMANCE_DIRECTION_Y = 1.02
MOTIVATION_DIRECTION_Y = PERFORMANCE_DIRECTION_Y
MMIO_DIRECTION_Y = PERFORMANCE_DIRECTION_Y
MMIO_LEGEND_ANCHOR = (0.5, 0.99)
MMIO_MAX_PLOT_SIZE_BYTES = 8

plt.rcParams['mathtext.fontset'] = 'cm'
plt.rcParams['font.size'] = FONT_SIZE
plt.rcParams['axes.labelsize'] = LABEL_SIZE
plt.rcParams['xtick.labelsize'] = TICK_SIZE
plt.rcParams['ytick.labelsize'] = TICK_SIZE
plt.rcParams['legend.fontsize'] = LEGEND_SIZE

def gmean(x):
    """Calculates the geometric mean of a series."""
    return np.exp(np.mean(np.log(x[x > 0])))

def size_unit(sizes):
    """Returns (unit_label, divisor) based on the smallest size in the list."""
    mn = min(sizes)
    if mn >= 1048576:
        return 'MiB', 1048576
    elif mn >= 1024:
        return 'KiB', 1024
    return 'B', 1

def display_variant_label(variant):
    """Returns the plot-facing name for an internal benchmark variant."""
    labels = {
        'Baseline': 'baseline',
        'Baseline (BM)': 'baseline (BM)',
        'Jigsaw': 'jigsaw',
        'Jigsaw (BM)': 'jigsaw (BM)',
        'Jigsaw-minus-NW': 'jigsaw-minus-nw',
        'Jigsaw-minus-NW (BM)': 'jigsaw-minus-nw (BM)',
        'CPU-attached': 'cpu-attached',
        'SW-Forwarder': 'sw-forwarder',
    }
    return labels.get(variant, variant)

def display_legend_label(variant, op=None):
    label = display_variant_label(variant)
    if op:
        return f"{label} {op.upper()}"
    return label

def limit_mmio_plot_size(df):
    """Keeps MMIO figures focused on byte-sized access widths."""
    if df.empty:
        return df
    return df[df['size'] <= MMIO_MAX_PLOT_SIZE_BYTES]

def parse_logs(folder, label_file_map):
    """Parses benchmark logs and returns a DataFrame with the results."""
    mmio_data = []
    dma_data = []
    comp_data = []
    
    for label, filename in label_file_map.items():
        path = os.path.join(folder, filename)
        if not os.path.exists(path):
            # Try with 'jigsaw-' prefix if not found
            if not filename.startswith('jigsaw-'):
                path = os.path.join(folder, f'jigsaw-{filename}')
            if not os.path.exists(path):
                print(f"Warning: {path} not found.")
                continue
            
        count_mmio = 0
        count_dma = 0
        count_comp = 0
        in_cpu_attached_results = False
        in_no_virt_dma_results = False
        
        with open(path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line: continue

                if line.startswith("Results (size, direction, iteration, latency [us], throughput"):
                    in_cpu_attached_results = True
                    continue
                if line.startswith("Results (DMA-only) (size, direction, iteration, latency [us], mmio_setup [us], throughput"):
                    in_no_virt_dma_results = True
                    continue
                if line.startswith("Results (Computation)"):
                    in_no_virt_dma_results = False
                
                # --- Format 1: VM Logs (CSV prefix) ---
                # Example: [   18.226187] MMIO_BENCH_CSV: 1,write,11240,0
                mmio_match = re.search(r'MMIO_BENCH_CSV:\s*(\d+),\s*(\w+),\s*(\d+(?:\.\d+)?),\s*(\d+)', line)
                if mmio_match:
                    g = mmio_match.groups()
                    mmio_data.append({
                        'variant': label,
                        'op': g[1],
                        'size': int(g[0]),
                        'val': float(g[2])
                    })
                    count_mmio += 1
                    
                # Example: [   18.329276] DMA_BENCH_CSV: 4096,h2d,51311,25281,0,0.074
                dma_match = re.search(r'DMA_BENCH_CSV:\s*(\d+),\s*(\w+),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?),\s*(\d+),\s*(\d+(?:\.\d+)?)', line)
                if dma_match:
                    g = dma_match.groups()
                    dma_data.append({
                        'variant': label,
                        'op': g[1],
                        'size': int(g[0]),
                        'val': float(g[5]), # throughput
                        'latency_ns': float(g[2]),
                        'mmio_done_ns': float(g[3])
                    })
                    count_dma += 1

                # Example: [   18.541818] COMP_BENCH_CSV: 4096,100,comp,74080,49520,0
                comp_match = re.search(r'COMP_BENCH_CSV:\s*(\d+),\s*(\d+),\s*(\w+),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?),\s*(\d+)', line)
                if comp_match:
                    g = comp_match.groups()
                    comp_data.append({
                        'variant': label,
                        'size': int(g[0]),
                        'cycles': int(g[1]),
                        'op': g[2],
                        'time_ns': float(g[3]),
                        'mmio_done_ns': float(g[4])
                    })
                    count_comp += 1
                
                # --- Format 2: No-Virt Logs (Direct Output) ---
                # Example: 4096,h2d,77139,0,0.0494523 (size, direction, time_ns, run_id, throughput)

                if in_cpu_attached_results:
                    cpu_attached_dma = re.match(r'^(\d+),\s*(h2d|d2h),\s*(\d+),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?)$', line)
                    if cpu_attached_dma:
                        g = cpu_attached_dma.groups()
                        dma_data.append({
                            'variant': label,
                            'op': g[1],
                            'size': int(g[0]),
                            'val': float(g[4]), # throughput
                            'latency_ns': float(g[3]) * 1000.0, # latency_us
                            'mmio_done_ns': 0.0
                        })
                        count_dma += 1
                        continue

                if in_no_virt_dma_results:
                    no_virt_dma_us = re.match(r'^(\d+),\s*(h2d|d2h),\s*(\d+),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?)$', line)
                    if no_virt_dma_us:
                        g = no_virt_dma_us.groups()
                        dma_data.append({
                            'variant': label,
                            'op': g[1],
                            'size': int(g[0]),
                            'val': float(g[5]), # throughput
                            'latency_ns': float(g[3]) * 1000.0, # latency_us
                            'mmio_done_ns': float(g[4]) * 1000.0 # mmio_setup_us
                        })
                        count_dma += 1
                        continue
                
                # Try 6-column format (if ever used)
                no_virt_dma6 = re.match(r'^(\d+),\s*(h2d|d2h),\s*(\d+(?:\.\d+)?),\s*(\d+),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?)$', line)
                # Try 5-column format (Used in current logs)
                no_virt_dma5 = re.match(r'^(\d+),\s*(h2d|d2h),\s*(\d+(?:\.\d+)?),\s*(\d+),\s*(\d+(?:\.\d+)?)$', line)
                
                if no_virt_dma6:
                    g = no_virt_dma6.groups()
                    dma_data.append({
                        'variant': label, 'op': g[1], 'size': int(g[0]),
                        'val': float(g[5]), # throughput
                        'latency_ns': float(g[2]), # time_ns
                        'mmio_done_ns': float(g[4]) # mmio_done_ns
                    })
                    count_dma += 1
                elif no_virt_dma5:
                    g = no_virt_dma5.groups()
                    dma_data.append({
                        'variant': label, 'op': g[1], 'size': int(g[0]),
                        'val': float(g[4]), # throughput
                        'latency_ns': float(g[2]), # time_ns (already in ns)
                        'mmio_done_ns': 0.0
                    })
                    count_dma += 1
                
                # COMP No-Virt: size, cycles, iteration, time_ns, mmio_setup_ns, throughput
                # Example: 4096, 100, 0, 15340, 5200, 0.250
                no_virt_comp = re.match(r'^(\d+),\s*(\d+),\s*(\d+),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?),\s*(\d+(?:\.\d+)?)$', line)
                if no_virt_comp:
                    g = no_virt_comp.groups()
                    comp_data.append({
                        'variant': label,
                        'size': int(g[0]),
                        'cycles': int(g[1]),
                        'op': 'comp',
                        'time_ns': float(g[3]),
                        'mmio_done_ns': float(g[4])
                    })
                    count_comp += 1
        
        print(f"Parsed {label}: file={filename}, MMIO={count_mmio}, DMA={count_dma}, COMP={count_comp}")

                    
    df_mmio = pd.DataFrame(mmio_data)
    df_dma = pd.DataFrame(dma_data)
    df_comp = pd.DataFrame(comp_data)
    
    return df_mmio, df_dma, df_comp

def plot_lines(df, title, ylabel, output_path, variants, is_latency=False, two_row_legend=False, variant_linestyles=None, variant_markers=None, legend_left=False, direction_y=None, font_sizes=None, fig_size=None, y_max=None, single_row_legend=False, legend_loc=None, legend_ncol=None, legend_bbox_to_anchor=None, ax=None, subplot_label=None, split_op_legend=False):
    """
    General plotting function based on the user-provided style.
    
    X-axis: Data Size
    Groups: (Variant, Operation)
    """
    if df.empty:
        print(f"No data for {title} ({os.path.basename(output_path)}), skipping.")
        return

    label_size = LABEL_SIZE
    tick_size = TICK_SIZE
    legend_size = LEGEND_SIZE
    annotation_size = ANNOTATION_SIZE
    if font_sizes:
        label_size = font_sizes.get('label', label_size)
        tick_size = font_sizes.get('tick', tick_size)
        legend_size = font_sizes.get('legend', legend_size)
        annotation_size = font_sizes.get('annotation', annotation_size)

    # Data transformation: Geometric Mean
    stats = df.groupby(['size', 'variant', 'op'])['val'].agg([gmean]).reset_index()
    stats = stats.rename(columns={'gmean': 'mean'}) # Reuse naming for clarity
    
    if is_latency:
        stats['mean'] = stats['mean'] / 1000.0
    
    # Sort sizes for consistent X-axis
    sorted_sizes = sorted(stats['size'].unique())
    unit, divisor = size_unit(sorted_sizes)
    size_labels = [str(s // divisor) for s in sorted_sizes]
    
    # Force H2D to come before D2H in the legend
    ops = [o for o in ['h2d', 'd2h', 'comp'] if o in stats['op'].unique()]
    # Add any remaining ops
    ops += [o for o in stats['op'].unique() if o not in ops]
    
    # ===== CORE STYLE MAPPINGS =====
    # Colors by Core Variant
    def get_variant_color(v):
        v_lower = v.lower()
        if 'minus' in v_lower: 
            return 'tab:orange'
        if 'jigsaw' in v_lower: 
            return 'tab:green'
        if 'remote' in v_lower or 'cpu-attached' in v_lower: # Remote CPU
            return 'tab:red'
        # Check 'Baseline' (specifically the Baseline version)
        if 'baseline' in v_lower or 'cpu' in v_lower: 
            return 'tab:blue'
        return 'gray'

    # Line Styles by Environment
    def get_variant_linestyle(v):
        if variant_linestyles and v in variant_linestyles:
            return variant_linestyles[v]
        if '(BM)' in v or 'minus' in v.lower(): return '--'
        return '-'

    # Markers by Operation
    op_markers = {'h2d': 'o', 'd2h': 's', 'comp': '^'}

    def get_marker(v, o):
        if variant_markers and v in variant_markers:
            vm = variant_markers[v]
            if isinstance(vm, dict):
                return vm.get(o, 'x')
            return vm
        return op_markers.get(o, 'x')
    
    def get_variant_priority(v):
        """Hierarchical priority: Core Variant (0-2) then Environment (0-1)."""
        # Baseline=0, Minus NW=1, Jigsaw=2
        v_lower = v.lower()
        if 'minus' in v_lower:
            core_rank = 1
        elif 'jigsaw' in v_lower:
            core_rank = 2
        else: # Baseline
            core_rank = 0
            
        # Environment Rank (BM first, then VM)
        if '(BM)' in v or '(native)' in v.lower() or 'minus' in v.lower() or 'Motivation' in v:
            env_rank = 0 # Native
        else:
            env_rank = 1 # Virtualized
        return (core_rank, env_rank)

    sorted_variants = sorted(variants, key=get_variant_priority)

    # ===== PLOT SETUP =====
    standalone = ax is None
    if standalone:
        fig, ax = plt.subplots(figsize=fig_size or (10, 5))
    
    # ===== PLOTTING LINES =====
    for o in ops:
        for v in sorted_variants:
            v_color = get_variant_color(v)
            v_ls = get_variant_linestyle(v)
            
            sub = stats[(stats['variant'] == v) & (stats['op'] == o)].sort_values('size')
            if sub.empty: continue
            
            x_indices = [sorted_sizes.index(s) for s in sub['size']]
            ax.plot(x_indices, sub['mean'],
                    label=display_legend_label(v, o),
                    color=v_color, linestyle=v_ls,
                    marker=get_marker(v, o), markersize=12,
                    linewidth=2, markeredgecolor='k', alpha=0.9)
            
    # ===== LEGEND =====
    if split_op_legend:
        from matplotlib.lines import Line2D
        variant_handles, variant_labels_list = [], []
        for v in sorted_variants:
            if any(not stats[(stats['variant'] == v) & (stats['op'] == o)].empty for o in ops):
                variant_handles.append(Line2D([0], [0], color=get_variant_color(v),
                                              linestyle=get_variant_linestyle(v), linewidth=2))
                variant_labels_list.append(display_variant_label(v))
        op_marker_map = {'h2d': 'o', 'd2h': 's', 'write': 'o', 'read': 's'}
        op_handles = [Line2D([0], [0], color='gray', linestyle='',
                             marker=op_marker_map[o], markersize=12, markeredgecolor='k')
                      for o in ops if o in op_marker_map]
        op_labels_list = [o.upper() for o in ops if o in op_marker_map]
        all_handles = variant_handles + op_handles
        all_labels  = variant_labels_list + op_labels_list
        ncol = legend_ncol or 2
        loc  = 'upper left' if legend_left else 'lower center'
        bbox = None if legend_left else (0.5, 1.1)
        ax.legend(all_handles, all_labels, loc=loc, bbox_to_anchor=bbox,
                  ncol=ncol, frameon=True, fontsize=legend_size,
                  labelspacing=0.3, handletextpad=0.4, columnspacing=0.8)
    else:
        def legend_sort_key(label):
            op_part = label.split()[-1].lower()
            v_part = " ".join(label.split()[:-1])
            v_priority = get_variant_priority(v_part)
            o_priority = ops.index(op_part) if op_part in ops else 99
            return (v_priority, o_priority)

        handles, labels = ax.get_legend_handles_labels()
        sorted_legend = sorted(zip(handles, labels), key=lambda x: legend_sort_key(x[1]))
        if sorted_legend:
            new_handles, new_labels = zip(*sorted_legend)
            if legend_loc:
                if single_row_legend:
                    ncol = len(new_labels)
                    ax.legend(new_handles, new_labels, loc='upper left',
                              bbox_to_anchor=(0.0, 0.99, 1.0, 0.0),
                              ncol=ncol, mode='expand', frameon=True, fontsize=legend_size)
                else:
                    ncol = legend_ncol or min(2, len(new_labels))
                    ax.legend(new_handles, new_labels, loc=legend_loc,
                              bbox_to_anchor=legend_bbox_to_anchor,
                              ncol=ncol, frameon=True, fontsize=legend_size)
            elif legend_left:
                if single_row_legend:
                    ncol = len(new_labels)
                elif two_row_legend:
                    ncol = (len(new_labels) + 1) // 2
                elif legend_ncol:
                    ncol = legend_ncol
                else:
                    ncol = 1
                ax.legend(new_handles, new_labels, loc='upper left',
                          ncol=ncol, frameon=True, fontsize=legend_size)
            else:
                if single_row_legend:
                    ncol = len(new_labels)
                elif two_row_legend:
                    ncol = (len(new_labels) + 1) // 2
                else:
                    ncol = min(4, len(new_labels))
                ax.legend(new_handles, new_labels, loc='lower center', bbox_to_anchor=(0.5, 1.1),
                          ncol=ncol, frameon=True, fontsize=legend_size)
        else:
            ax.legend(loc='lower center', bbox_to_anchor=(0.5, 1.1), ncol=1, frameon=True,
                      fontsize=legend_size)

    # Axis Labels
    direction = "Lower is better ↓" if is_latency else "Higher is better ↑"
    top_label = subplot_label if subplot_label is not None else f'({direction})'
    ax.text(0.5, 1.0, top_label, transform=ax.transAxes,
            ha='center', va='bottom', fontsize=annotation_size, color='navy', fontweight='bold',
            clip_on=False)

    ax.set_xlabel(f"Data Size ({unit})", fontsize=label_size)
    ax.set_ylabel(ylabel, fontsize=label_size)
    
    # X-axis
    ax.set_xticks(range(len(sorted_sizes)))
    ax.set_xticklabels(size_labels, ha='center', fontsize=tick_size)
    
    ax.set_xticks(range(len(sorted_sizes)))
    ax.set_xticklabels(size_labels, fontsize=tick_size)
    ax.tick_params(axis='y', labelsize=tick_size)
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f'{x:g}'))
    if y_max is not None:
        ax.set_ylim(bottom=0, top=y_max)
    ax.grid(True, alpha=0.3)

    if standalone:
        plt.tight_layout()
        plt.savefig(output_path + ".png", dpi=300, bbox_inches='tight')
        plt.savefig(output_path + ".pdf", bbox_inches='tight')
        print(f"Saved {output_path}.png/pdf")
        plt.close()

def plot_4k_grouped_latency(df_dma, folder):
    """Grouped stacked bar plot for 4KiB DMA latency (Native vs Virtualized)."""
    if df_dma.empty: return
    
    # Selection for 4KiB H2D
    sub = df_dma[(df_dma['size'] == 4096) & (df_dma['op'] == 'h2d')].copy()
    if sub.empty: return
    
    # 1. Define Groups and Variants
    groups = {
        'Baseline': ['Baseline (BM)', 'Baseline'],
        'Jigsaw-minus-NW': ['Jigsaw-minus-NW (BM)', 'Jigsaw-minus-NW'],
        'Jigsaw': ['Jigsaw (BM)', 'Jigsaw']
    }
    
    # 2. Get Statistics
    stats = sub.groupby('variant').agg({
        'latency_ns': gmean,
        'mmio_done_ns': gmean
    }).reset_index()
    
    # 3. Preparation for Plotting
    fig, ax = plt.subplots(figsize=SINGLE_COLUMN_FIGURE_SIZE)

    group_labels = list(groups.keys())
    y = np.arange(len(group_labels))
    height = 0.30  # height of the horizontal bars

    # Color mapping (Init, Exec)
    colors = ('tab:blue', 'tab:green')

    for i, (group, variants) in enumerate(groups.items()):
        # Bare Metal Bar (Bottom offset) - No hatch
        v_native = variants[0]
        row_n = stats[stats['variant'] == v_native]
        if not row_n.empty:
            init = row_n['mmio_done_ns'].values[0] / 1000.0
            exec_t = (row_n['latency_ns'].values[0] - row_n['mmio_done_ns'].values[0]) / 1000.0
            total = init + exec_t

            ax.barh(y[i] - height/2, init, height, color=colors[0], alpha=0.8, edgecolor='k', label='Init (Bare Metal)' if i==0 else "")
            ax.barh(y[i] - height/2, exec_t, height, left=init, color=colors[1], alpha=0.8, edgecolor='k', label='Exec (Bare Metal)' if i==0 else "")
            ax.text(total + 0.2, y[i] - height/2, f'{round(total)}', va='center', ha='left',
                    fontsize=SINGLE_COLUMN_FONT_SIZES['annotation'] + 1, fontweight='bold')

        # VM Bar (Top offset) - With hatch
        v_virt = variants[1]
        row_v = stats[stats['variant'] == v_virt]
        if not row_v.empty:
            init = row_v['mmio_done_ns'].values[0] / 1000.0
            exec_t = (row_v['latency_ns'].values[0] - row_v['mmio_done_ns'].values[0]) / 1000.0
            total = init + exec_t

            ax.barh(y[i] + height/2, init, height, color=colors[0], alpha=0.8, edgecolor='k', label='Init (VM)' if i==0 else "", hatch='///')
            ax.barh(y[i] + height/2, exec_t, height, left=init, color=colors[1], alpha=0.8, edgecolor='k', label='Exec (VM)' if i==0 else "", hatch='///')
            ax.text(total + 0.2, y[i] + height/2, f'{round(total)}', va='center', ha='left',
                    fontsize=SINGLE_COLUMN_FONT_SIZES['annotation'] + 1, fontweight='bold')

    # 4. Labeling and Aesthetics
    local_label_size = SINGLE_COLUMN_FONT_SIZES['label'] + 4
    local_tick_size = SINGLE_COLUMN_FONT_SIZES['tick'] + 4
    local_legend_size = SINGLE_COLUMN_FONT_SIZES['legend'] + 4
    local_annotation_size = SINGLE_COLUMN_FONT_SIZES['annotation'] + 1

    ax.set_xlabel("Latency [us]", fontsize=local_label_size)
    ax.set_yticks(y)
    ax.set_yticklabels(group_labels, fontsize=local_tick_size)
    ax.tick_params(axis='x', labelsize=local_tick_size)

    ax.legend(loc='lower right', ncol=1, frameon=True, fontsize=local_legend_size)
    ax.grid(True, axis='x', alpha=0.3)

    ax.text(1.0, 0.5, '← Lower is better',
            transform=ax.transAxes, ha='right', va='center',
            fontsize=local_annotation_size, color='navy', fontweight='bold')

    plt.tight_layout()
    out_path = os.path.join(folder, "DMA_Latency_Decomposition_4K")
    plt.savefig(out_path + ".png", dpi=300, bbox_inches='tight')
    plt.savefig(out_path + ".pdf", bbox_inches='tight')
    print(f"Saved {out_path}.png/pdf")
    plt.close()

def plot_comp_performance(df, folder, variants, font_sizes=None, fig_size=None, direction_y=0.90, ax=None, subplot_label=None, ylabel="Relative Perf."):
    """
    Plots computation performance: Time vs Data Size for different cycle counts.
    Cycles are converted to time (1 cycle = 4ns).
    """
    if df.empty:
        print("No computation data found.")
        return

    label_size = LABEL_SIZE
    tick_size = TICK_SIZE
    legend_size = LEGEND_SIZE
    annotation_size = ANNOTATION_SIZE
    if font_sizes:
        label_size = font_sizes.get('label', label_size)
        tick_size = font_sizes.get('tick', tick_size)
        legend_size = font_sizes.get('legend', legend_size)
        annotation_size = font_sizes.get('annotation', annotation_size)

    # Convert cycles to time label (us)
    df['comp_time_us'] = df['cycles'] * 4 / 1000.0
    
    # Calculate geometric mean
    stats = df.groupby(['size', 'variant', 'cycles', 'comp_time_us'])['time_ns'].agg([gmean]).reset_index()
    stats = stats.rename(columns={'gmean': 'mean'})
    
    sorted_sizes = sorted(stats['size'].unique())
    unit, divisor = size_unit(sorted_sizes)
    size_labels = [str(s // divisor) for s in sorted_sizes]

    unique_cycles = sorted(stats['cycles'].unique())
    
    standalone = ax is None
    if standalone:
        fig, ax = plt.subplots(figsize=fig_size or (10, 5))
    palette = sns.color_palette("husl", len(unique_cycles))
    markers = ['o', 's', '^', 'D', 'v', 'p']
    
    for i, cycles in enumerate(unique_cycles):
        comp_us = cycles * 4 / 1000.0
        label_time = f"{comp_us:.0f}us" if comp_us >= 1 else f"{comp_us*1000:.0f}ns"
        
        sub_base = stats[(stats['cycles'] == cycles) & (stats['variant'] == 'Baseline')].sort_values('size')
        sub_comp = stats[(stats['cycles'] == cycles) & (stats['variant'] == 'Jigsaw')].sort_values('size')
        
        if not sub_base.empty and not sub_comp.empty:
            merged = pd.merge(sub_base, sub_comp, on='size', suffixes=('_base', '_comp'))
            merged['ratio'] = merged['mean_comp'] / merged['mean_base']
            
            x_indices = [sorted_sizes.index(s) for s in merged['size']]
            
            ax.plot(x_indices, merged['ratio'], 
                    label=label_time,
                    color=palette[i],
                    marker=markers[i % len(markers)],
                    markersize=12, linewidth=2)

    ax.set_xlabel(f"Data Size ({unit})", fontsize=label_size)
    ax.set_ylabel(ylabel, fontsize=label_size)
    ax.set_xticks(range(len(sorted_sizes)))
    ax.set_xticklabels(size_labels, fontsize=tick_size)
    ax.tick_params(axis='y', labelsize=tick_size)
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f'{x:g}'))
    ax.grid(True, alpha=0.3)

    ax.legend(loc='upper right', ncol=1,
              frameon=True, fontsize=legend_size)
    top_label = subplot_label if subplot_label is not None else "(Lower is better ↓)"
    ax.text(0.5, 1.0, top_label, transform=ax.transAxes,
            ha='center', va='bottom', fontsize=annotation_size, color='navy', fontweight='bold',
            clip_on=False)
    
    if standalone:
        plt.tight_layout()
        plt.savefig(os.path.join(folder, "Computation_Performance.png"), dpi=300, bbox_inches='tight')
        plt.savefig(os.path.join(folder, "Computation_Performance.pdf"), bbox_inches='tight')
        print("Saved Computation_Performance.png/pdf")
        plt.close()

def read_cycles_scale(path):
    """Reads the cycles_scale a log was captured with (TRACE_CSV line).

    Falls back to a -cs<N> filename suffix, then 1 (as-captured traces)."""
    try:
        with open(path, 'r', errors='replace') as f:
            for line in f:
                m = re.search(r'TRACE_CSV:\s*cycles_scale=(\d+)', line)
                if m:
                    return int(m.group(1))
    except OSError:
        pass
    m = re.search(r'-cs(\d+)\.log$', path)
    return int(m.group(1)) if m else 1

def parse_app_summary(folder, label_file_map):
    """Parses per-app summary from logs.

    For every mapped file, scale-suffixed siblings (<stem>-cs<N>.log) are
    parsed too; every row is tagged with the cycles_scale its log reports
    (TRACE_CSV: cycles_scale=N) so plots can be emitted per scale."""
    data = []
    expanded = []
    for label, filename in label_file_map.items():
        stem, ext = os.path.splitext(filename)
        siblings = sorted(os.path.basename(p) for p in
                          glob.glob(os.path.join(folder, f"{stem}-cs*{ext}")))
        candidates = [filename] + siblings
        if not any(os.path.exists(os.path.join(folder, c)) for c in candidates):
            print(f"Warning: {os.path.join(folder, filename)} not found.")
            continue
        expanded += [(label, c) for c in candidates
                     if os.path.exists(os.path.join(folder, c))]

    for label, filename in expanded:
        path = os.path.join(folder, filename)
        cycles_scale = read_cycles_scale(path)

        in_summary = False
        with open(path, 'r') as f:
            for line in f:
                line = line.strip()
                trace_match = re.search(r'TRACE_SUMMARY:\s*(.*)$', line)
                if trace_match:
                    parts = [p.strip() for p in trace_match.group(1).split(',')]
                    if not parts or parts[0] in ('app', 'set'):
                        continue

                    if len(parts) >= 11 and parts[2].isdigit():
                        set_name = parts[0]
                        app_name = parts[1]
                        total_ns_idx = 10
                    elif len(parts) >= 10 and parts[1].isdigit():
                        set_name = 'all'
                        app_name = parts[0]
                        total_ns_idx = 9
                    else:
                        continue

                    if app_name == 'oclprintf':
                        continue
                    data.append({
                        'set': set_name,
                        'variant': label,
                        'app': app_name,
                        'cycles_scale': cycles_scale,
                        'total_us': float(parts[total_ns_idx]) / 1000.0
                    })
                    continue

                if "# per-app summary" in line:
                    in_summary = True
                    next(f) # skip header
                    continue
                if "ASYMMETRIC DMA TEST" in line:
                    in_summary = False
                    break
                
                if in_summary and line:
                    parts = [p.strip() for p in line.split(',')]
                    if len(parts) >= 9:
                        app_name = parts[0]
                        if app_name == 'oclprintf': continue
                        data.append({
                            'set': 'all',
                            'variant': label,
                            'app': app_name,
                            'cycles_scale': cycles_scale,
                            'total_us': float(parts[8])
                        })
    df = pd.DataFrame(data)
    if df.empty:
        return df
    return df.groupby(['set', 'variant', 'app', 'cycles_scale'],
                      as_index=False)['total_us'].agg(gmean)

def parse_no_virt_app_summary(folder, label_file_map):
    """Parses no-virt per-app summary tables.

    Native logs print a CSV table after "# per-app summary" with columns:
    app, run, ..., total_bundle_us, total_us, ...
    """
    data = []
    for label, filename in label_file_map.items():
        path = os.path.join(folder, filename)
        if not os.path.exists(path):
            print(f"Warning: {path} not found.")
            continue

        in_summary = False
        total_us_idx = None
        count = 0

        with open(path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                if "# per-app summary" in line:
                    in_summary = True
                    total_us_idx = None
                    continue

                if not in_summary:
                    continue

                if line.startswith("app,"):
                    header = [p.strip() for p in line.split(',')]
                    if "total_us" not in header:
                        print(f"Warning: total_us column not found in {filename}.")
                        in_summary = False
                        continue
                    total_us_idx = header.index("total_us")
                    continue

                parts = [p.strip() for p in line.split(',')]
                if total_us_idx is None or len(parts) <= total_us_idx:
                    continue

                app_name = parts[0]
                if app_name == 'oclprintf':
                    continue

                try:
                    total_us = float(parts[total_us_idx])
                except ValueError:
                    continue

                data.append({
                    'variant': label,
                    'app': app_name,
                    'total_us': total_us
                })
                count += 1

        print(f"Parsed no-virt apps {label}: file={filename}, rows={count}")

    df = pd.DataFrame(data)
    if df.empty:
        return df
    return df.groupby(['variant', 'app'], as_index=False)['total_us'].agg(gmean)

def summarize_app_set_changes(df, folder, name_suffix=''):
    """Writes a per-app summary of Jigsaw/Baseline ratios across trace sets."""
    if df.empty or 'set' not in df.columns:
        return pd.DataFrame()

    ratio_df = df.pivot_table(index=['set', 'app'],
                              columns='variant',
                              values='total_us',
                              aggfunc='first').reset_index()
    if 'Baseline' not in ratio_df.columns or 'Jigsaw' not in ratio_df.columns:
        return pd.DataFrame()

    ratio_df['jigsaw_over_baseline'] = ratio_df['Jigsaw'] / ratio_df['Baseline']
    ratio_wide = ratio_df.pivot(index='app', columns='set', values='jigsaw_over_baseline')

    preferred_sets = ['small', 'medium', 'long']
    present_sets = [s for s in preferred_sets if s in ratio_wide.columns]
    present_sets += sorted(s for s in ratio_wide.columns if s not in present_sets and s != 'all')
    if not present_sets:
        return pd.DataFrame()

    summary = ratio_wide[present_sets].copy()
    summary = summary.rename(columns={s: f'ratio_{s}' for s in present_sets})
    ratio_cols = [f'ratio_{s}' for s in present_sets]
    summary['min_ratio'] = summary[ratio_cols].min(axis=1)
    summary['max_ratio'] = summary[ratio_cols].max(axis=1)
    summary['ratio_spread'] = summary['max_ratio'] - summary['min_ratio']

    if all(f'ratio_{s}' in summary.columns for s in preferred_sets):
        summary['trend'] = np.select(
            [
                (summary['ratio_small'] >= summary['ratio_medium']) &
                (summary['ratio_medium'] >= summary['ratio_long']),
                (summary['ratio_small'] <= summary['ratio_medium']) &
                (summary['ratio_medium'] <= summary['ratio_long'])
            ],
            ['decreases', 'increases'],
            default='mixed'
        )

    summary = summary.reset_index()
    sort_cols = [c for c in ['ratio_small', 'ratio_spread'] if c in summary.columns]
    ascending = [True, False][:len(sort_cols)]
    if sort_cols:
        summary = summary.sort_values(sort_cols, ascending=ascending)

    out_path = os.path.join(folder, f"E2E_App_Performance_set_summary{name_suffix}.csv")
    summary.to_csv(out_path, index=False, float_format='%.6f')
    print(f"Saved {out_path}")
    return summary

def plot_app_performance(df, folder, set_name=None, app_order=None, name_suffix=''):
    """Bar plot for per-app performance with custom styling."""
    if df.empty: return

    if 'set' in df.columns and set_name is None:
        baseline_label = 'Baseline'
        preferred_sets = ['small', 'medium', 'long']
        present_sets = list(df['set'].dropna().unique())
        ordered_sets = [s for s in preferred_sets if s in present_sets]
        ordered_sets += sorted(s for s in present_sets if s not in ordered_sets)

        small_df = df[df['set'] == 'small']
        small_ratios = small_df.pivot_table(index='app',
                                            columns='variant',
                                            values='total_us',
                                            aggfunc='first')
        if baseline_label in small_ratios.columns and 'Jigsaw' in small_ratios.columns:
            app_order = (small_ratios.assign(
                            overhead_ratio=small_ratios['Jigsaw'] / small_ratios[baseline_label])
                         .sort_values('overhead_ratio', ascending=True)
                         .index
                         .tolist())
        else:
            app_order = sorted(df['app'].unique())

        if ordered_sets == ['all']:
            plot_app_performance(df.drop(columns=['set']), folder, set_name='all',
                                 app_order=app_order, name_suffix=name_suffix)
            return

        for current_set in ordered_sets:
            plot_app_performance(
                df[df['set'] == current_set].drop(columns=['set']),
                folder,
                set_name=current_set,
                app_order=app_order,
                name_suffix=name_suffix
            )
        return

    baseline_label = 'Baseline'
    variant_order = ['Baseline', 'Jigsaw', 'SW-Forwarder']
    variants = [v for v in variant_order if v in df['variant'].unique()]

    if app_order is None:
        app_order = df[df['variant'] == baseline_label].sort_values('total_us', ascending=False)['app'].tolist()
        if not app_order:
            app_order = sorted(df['app'].unique())

    # Trailing geomean group: one synthetic "app" whose per-variant value is
    # the geomean of that variant's per-app totals (over the ordered apps).
    geo_rows = []
    for variant in df['variant'].unique():
        sub = df[df['variant'] == variant].set_index('app')['total_us']
        vals = sub.reindex(app_order).dropna()
        if len(vals):
            geo_rows.append({'app': 'geomean', 'variant': variant,
                             'total_us': gmean(vals)})
    df = pd.concat([df, pd.DataFrame(geo_rows)], ignore_index=True)
    app_order = list(app_order) + ['geomean']

    df['app'] = pd.Categorical(df['app'], categories=app_order, ordered=True)
    df = df.sort_values('app')

    # ===== STYLE SETUP =====
    local_label_size = LABEL_SIZE + 4
    local_tick_size = TICK_SIZE + 4
    local_legend_size = LEGEND_SIZE + 4
    local_speedup_size = SPEEDUP_SIZE + 4

    palette = sns.color_palette("pastel")
    # Blue, green, red-ish for baseline / jigsaw / sw forwarder
    colors = {'Baseline': palette[0], 'Jigsaw': palette[2], 'SW-Forwarder': palette[3]}

    fig, ax = plt.subplots(figsize=(16.0, 3.5))

    apps = list(df['app'].cat.categories)
    x = np.arange(len(apps))
    bar_width = min(0.34, 0.85 / max(len(variants), 1))

    hatches = {'Baseline': '', 'Jigsaw': '\\\\', 'SW-Forwarder': '//'}
    baseline_by_app = df[df['variant'] == baseline_label].set_index('app')['total_us']

    for i, variant in enumerate(variants):
        sub = df[df['variant'] == variant]
        if sub.empty: continue

        # Ensure we match the order of apps
        sub = sub.set_index('app').reindex(apps).reset_index()

        bars = ax.bar(x + (i - (len(variants) - 1) / 2.0) * bar_width, sub['total_us'], bar_width,
                      label=display_variant_label(variant), color=colors[variant],
                      hatch=hatches[variant], edgecolor='black', linewidth=1, alpha=1.0)

        if variant != baseline_label:
            # Stagger annotation rows per variant so near-equal bars don't
            # print their ratios on top of each other.
            row = variants.index(variant) - 1
            for bar, app_name, total_us in zip(bars, sub['app'], sub['total_us']):
                if pd.isna(total_us) or app_name not in baseline_by_app:
                    continue
                baseline_us = baseline_by_app.loc[app_name]
                if pd.isna(baseline_us) or baseline_us <= 0:
                    continue
                ax.annotate(f'{total_us / baseline_us:.2f}x',
                            xy=(bar.get_x() + bar.get_width() / 2, total_us),
                            xytext=(0, 3 + row * (local_speedup_size + 4)),
                            textcoords='offset points',
                            ha='center', va='bottom',
                            fontsize=local_speedup_size)

    # ===== AXIS FORMATTING =====
    # separator before the trailing geomean group
    ax.axvline(len(apps) - 1.5, color='gray', linewidth=1,
               linestyle='--', alpha=0.7)
    ax.set_ylabel('')
    # Custom y-axis label (rotated)
    ax.text(-0.04, 0.5, 'Execution Time [us]',
            transform=ax.transAxes,
            rotation=90, ha='center', va='center',
            fontsize=local_label_size)

    ax.set_xlabel('')
    ax.set_xticks(x)
    ax.set_xticklabels(apps, rotation=45, ha='right', fontsize=local_tick_size)
    ax.tick_params(axis='y', labelsize=local_tick_size)
    ax.set_yscale('log')
    ax.set_ylim(top=df['total_us'].max() * (2.2 if len(variants) <= 2 else 4.0))

    # Grid
    ax.grid(True, alpha=0.3, axis='y', color='gray', linestyle='-')
    ax.set_axisbelow(True)

    # Legend
    ax.legend(loc='upper right', ncol=len(variants), frameon=True,
              fontsize=local_legend_size, columnspacing=1.5)
    
    plt.tight_layout()
    plt.margins(x=0.015, y=0.0, tight=True)
    
    output_name = "E2E_App_Performance"
    if set_name and set_name != 'all':
        output_name = f"{output_name}-{set_name}"
    output_name = f"{output_name}{name_suffix}"
    out_path = os.path.join(folder, output_name)
    plt.savefig(out_path + ".png", dpi=300, bbox_inches='tight')
    plt.savefig(out_path + ".pdf", bbox_inches='tight')
    print(f"Saved {out_path}.png/pdf")
    plt.close()

def plot_compact_app_size_performance(df, folder, name_suffix=''):
    """Compact E2E app plot with size subgroups for selected applications."""
    if df.empty or 'set' not in df.columns:
        return

    selected_apps = [
        'vecadd',
        'sfilter',
        'saxpy',
        'psum',
        'sgemm2',
        'conv3',
        'transpose',
        'kmeans',
    ]
    set_order = [('small', 'Small'), ('medium', 'Medium'), ('long', 'Large')]
    variants = ['Baseline', 'Jigsaw']

    df = df[df['app'].isin(selected_apps) &
            df['set'].isin([s for s, _ in set_order]) &
            df['variant'].isin(variants)].copy()
    if df.empty:
        print("No compact app size data found.")
        return

    app_order = [app for app in selected_apps if app in set(df['app'])]
    df['app'] = pd.Categorical(df['app'], categories=app_order, ordered=True)
    df = df.sort_values('app')

    local_label_size = LABEL_SIZE + 4
    local_tick_size = TICK_SIZE + 4
    local_legend_size = LEGEND_SIZE + 4
    local_annotation_size = 14  # was 8

    palette = sns.color_palette("pastel")
    colors = {'Baseline': palette[0], 'Jigsaw': palette[2]}
    hatches = {'small': '', 'medium': '///', 'long': '\\\\'}

    fig, ax = plt.subplots(figsize=(16.0, 3.5))

    apps = list(df['app'].cat.categories)
    x = np.arange(len(apps))
    bar_width = 0.095
    n_bars = len(set_order) * len(variants)
    offsets = (np.arange(n_bars) - (n_bars - 1) / 2.0) * bar_width

    baseline_lookup = (df[df['variant'] == 'Baseline']
                       .set_index(['set', 'app'])['total_us'])

    bar_idx = 0
    for set_name, set_label in set_order:
        for variant in variants:
            sub = (df[(df['set'] == set_name) & (df['variant'] == variant)]
                   .set_index('app')
                   .reindex(apps)
                   .reset_index())
            bars = ax.bar(x + offsets[bar_idx], sub['total_us'], bar_width,
                          label=f"{set_label} {display_variant_label(variant)}",
                          color=colors[variant],
                          hatch=hatches[set_name],
                          edgecolor='black',
                          linewidth=0.8,
                          alpha=1.0)

            if variant == 'Jigsaw':
                for bar, app_name, total_us in zip(bars, sub['app'], sub['total_us']):
                    key = (set_name, app_name)
                    if pd.isna(total_us) or key not in baseline_lookup:
                        continue
                    baseline_us = baseline_lookup.loc[key]
                    if pd.isna(baseline_us) or baseline_us <= 0:
                        continue
                    ax.annotate(f'{total_us / baseline_us:.2f}x',
                                xy=(bar.get_x() + bar.get_width() / 2 - bar_width, baseline_us),
                                xytext=(0, 8),
                                textcoords='offset points',
                                ha='center',
                                va='bottom',
                                fontsize=local_annotation_size)
            bar_idx += 1

    ax.set_ylabel('')
    ax.text(-0.045, 0.5, 'Execution Time [us]',
            transform=ax.transAxes,
            rotation=90,
            ha='center',
            va='center',
            fontsize=local_label_size)
    ax.set_xlabel('')
    ax.set_xticks(x)
    ax.set_xticklabels(apps, rotation=0, ha='center', fontsize=local_tick_size)
    ax.tick_params(axis='y', labelsize=local_tick_size)
    ax.set_yscale('log')
    ax.set_ylim(top=df['total_us'].max() * 3.2)
    ax.grid(True, alpha=0.3, axis='y', color='gray', linestyle='-')
    ax.set_axisbelow(True)

    ax.legend(loc='upper left',
              ncol=3,
              frameon=True,
              fontsize=local_legend_size,
              columnspacing=1.1)

    plt.tight_layout()
    plt.margins(x=0.015, y=0.0, tight=True)

    out_path = os.path.join(folder, f"E2E_App_Performance_Compact_By_Size{name_suffix}")
    plt.savefig(out_path + ".png", dpi=300, bbox_inches='tight')
    plt.savefig(out_path + ".pdf", bbox_inches='tight')
    print(f"Saved {out_path}.png/pdf")
    plt.close()

def plot_no_virt_app_performance(df, folder):
    """Bar plot for no-virt per-app performance."""
    if df.empty:
        print("No no-virt application data found.")
        return

    baseline_label = 'Baseline (BM)'
    variants = ['Baseline (BM)', 'Jigsaw (BM)']
    variants = [v for v in variants if v in set(df['variant'])]

    baseline_order = df[df['variant'] == baseline_label].sort_values('total_us', ascending=False)['app'].tolist()
    if not baseline_order:
        baseline_order = sorted(df['app'].unique())

    df = df.copy()
    df['app'] = pd.Categorical(df['app'], categories=baseline_order, ordered=True)
    df = df.sort_values('app')

    palette = sns.color_palette("pastel")
    colors = {
        'Baseline (BM)': palette[0],
        'Jigsaw (BM)': palette[2],
    }
    hatches = {
        'Baseline (BM)': '',
        'Jigsaw (BM)': '\\\\',
    }

    fig, ax = plt.subplots(figsize=(16.0, 3.8))

    apps = list(df['app'].cat.categories)
    x = np.arange(len(apps))
    bar_width = min(0.24, 0.78 / max(len(variants), 1))
    offsets = (np.arange(len(variants)) - (len(variants) - 1) / 2.0) * bar_width
    baseline_by_app = df[df['variant'] == baseline_label].set_index('app')['total_us']

    for i, variant in enumerate(variants):
        sub = df[df['variant'] == variant].set_index('app').reindex(apps).reset_index()
        bars = ax.bar(x + offsets[i], sub['total_us'], bar_width,
                      label=display_variant_label(variant), color=colors.get(variant, 'gray'),
                      hatch=hatches.get(variant, ''), edgecolor='black',
                      linewidth=1, alpha=1.0)

        if variant != baseline_label:
            for bar, app_name, total_us in zip(bars, sub['app'], sub['total_us']):
                if pd.isna(total_us) or app_name not in baseline_by_app:
                    continue
                baseline_us = baseline_by_app.loc[app_name]
                if pd.isna(baseline_us) or baseline_us <= 0:
                    continue
                ax.annotate(f'{total_us / baseline_us:.2f}x',
                            xy=(bar.get_x() + bar.get_width() / 2, total_us),
                            xytext=(0, 3), textcoords='offset points',
                            ha='center', va='bottom',
                            fontsize=SPEEDUP_SIZE,
                            rotation=90)

    ax.set_ylabel('')
    ax.text(-0.04, 0.5, 'Execution Time [us]',
            transform=ax.transAxes,
            rotation=90, ha='center', va='center',
            fontsize=LABEL_SIZE)

    ax.set_xlabel('')
    ax.set_xticks(x)
    ax.set_xticklabels(apps, rotation=45, ha='right', fontsize=TICK_SIZE)
    ax.set_yscale('log')
    ax.set_ylim(top=df['total_us'].max() * (2.2 if len(variants) <= 2 else 4.0))
    ax.grid(True, alpha=0.3, axis='y', color='gray', linestyle='-')
    ax.set_axisbelow(True)
    sns.despine(ax=ax)

    ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.15),
              ncol=len(variants), frameon=True, fontsize=LEGEND_SIZE,
              columnspacing=1.5)

    plt.tight_layout()
    plt.margins(x=0.015, y=0.0, tight=True)

    out_path = os.path.join(folder, "E2E_App_Performance_NoVirt")
    plt.savefig(out_path + ".png", dpi=300, bbox_inches='tight')
    plt.savefig(out_path + ".pdf", bbox_inches='tight')
    print(f"Saved {out_path}.png/pdf")
    plt.close()

def plot_gpu_stranding(data_dir, output_dir, min_sustained_hours=6,
                       ax=None, font_sizes=None, subplot_label=None):
    """Bar plot of max sustained GPU stranding per GPU type.

    For each CSV in *data_dir* (one per GPU type, one row per hour) this
    function identifies all consecutive runs of non-zero ``gpu_strand``
    values that last at least *min_sustained_hours*, then takes the maximum
    ``gpu_strand`` value observed in those sustained streaks.  The result is
    a single bar per GPU type.
    """
    import re, csv

    label_size = font_sizes.get('label', SINGLE_COLUMN_FONT_SIZES['label']) if font_sizes else SINGLE_COLUMN_FONT_SIZES['label']
    tick_size = font_sizes.get('tick', SINGLE_COLUMN_FONT_SIZES['tick']) if font_sizes else SINGLE_COLUMN_FONT_SIZES['tick']
    annotation_size = font_sizes.get('annotation', SINGLE_COLUMN_FONT_SIZES['annotation']) if font_sizes else SINGLE_COLUMN_FONT_SIZES['annotation']

    gpu_data = {}  # gpu_name -> max sustained stranding

    for fname in sorted(os.listdir(data_dir)):
        if not fname.endswith('.csv'):
            continue

        # Extract GPU name from filename: cluster_stats-<count>-<GPU_NAME>.csv
        m = re.match(r'cluster_stats-\d+-(.*?)\.csv', fname)
        gpu_name = m.group(1) if m else fname.replace('.csv', '')

        with open(os.path.join(data_dir, fname)) as fh:
            reader = csv.DictReader(fh)
            vals = [float(r['gpu_strand']) for r in reader]

        # Walk through the trace and collect sustained streaks
        max_sustained = 0.0
        curr_vals = []
        for v in vals:
            if v > 0:
                curr_vals.append(v)
            else:
                if len(curr_vals) >= min_sustained_hours:
                    max_sustained = max(max_sustained, max(curr_vals))
                curr_vals = []
        # Handle a streak that extends to the end of the trace
        if len(curr_vals) >= min_sustained_hours:
            max_sustained = max(max_sustained, max(curr_vals))

        gpu_data[gpu_name] = max_sustained

    if not gpu_data:
        print("No stranding data found, skipping.")
        return

    # Sort GPU types alphabetically for a consistent order
    gpu_names = sorted(gpu_data.keys())
    values = [gpu_data[g] for g in gpu_names]

    # ===== PLOT =====
    palette = sns.color_palette("pastel")
    standalone = ax is None
    if standalone:
        fig, ax = plt.subplots(figsize=SINGLE_COLUMN_FIGURE_SIZE)

    x = np.arange(len(gpu_names))
    bar_width = 0.35

    # Convert fractions to percentages for plotting
    pct_values = [v * 100.0 for v in values]

    bars = ax.bar(x, pct_values, bar_width,
                  color=palette[0], edgecolor='black', linewidth=1, alpha=1.0)

    # Annotate each bar with its percentage value
    for bar, pct in zip(bars, pct_values):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.5,
                f'{pct:.1f}%', ha='center', va='bottom',
                fontsize=annotation_size, fontweight='bold')

    # ===== AXIS FORMATTING =====
    ax.set_ylabel('Stranded GPUs (%)', fontsize=label_size)

    ax.set_xticks(x)
    ax.set_xticklabels(gpu_names, rotation=25, ha='right', fontsize=tick_size)
    ax.tick_params(axis='y', labelsize=tick_size)

    # Grid
    ax.set_ylim(0, 80)
    ax.grid(True, alpha=0.3, axis='y', color='gray', linestyle='-')
    ax.set_axisbelow(True)

    for spine in ax.spines.values():
        spine.set_visible(True)

    top_label = subplot_label if subplot_label is not None else ""
    if top_label:
        ax.text(0.5, 1.0, top_label, transform=ax.transAxes,
                ha='center', va='bottom', fontsize=annotation_size, color='navy',
                fontweight='bold', clip_on=False)

    if standalone:
        plt.tight_layout()
        plt.margins(x=0.015, y=0.0, tight=True)
        out_path = os.path.join(output_dir, "GPU_Stranding")
        plt.savefig(out_path + ".png", dpi=300, bbox_inches='tight')
        plt.savefig(out_path + ".pdf", bbox_inches='tight')
        print(f"Saved {out_path}.png/pdf")
        plt.close()

def plot_e2e_combined(df_mmio, df_dma, df_comp, folder, variants, markers,
                      font_sizes=None, fig_size=None):
    """Three e2e microbench plots side-by-side in one figure."""
    font_sizes = font_sizes or E2E_COMBINED_FONT_SIZES
    fig_size = fig_size or E2E_COMBINED_FIGURE_SIZE

    fig, axes = plt.subplots(1, 3, figsize=fig_size)

    plot_lines(limit_mmio_plot_size(df_mmio), "MMIO Latency", "Latency [us]",
               None, variants, is_latency=True,
               variant_markers=markers, legend_left=True,
               font_sizes=font_sizes, y_max=30, legend_ncol=4, split_op_legend=True,
               subplot_label="(a) MMIO perf (lower is better ↓)",
               ax=axes[0])

    plot_lines(df_dma, "DMA Throughput", "Throughput [GiB/s]",
               None, variants, is_latency=False,
               variant_markers=markers, legend_left=True,
               font_sizes=font_sizes, legend_ncol=2, split_op_legend=True,
               subplot_label="(b) DMA perf (higher is better ↑)",
               ax=axes[1])

    plot_comp_performance(df_comp, folder, variants,
                          font_sizes=font_sizes,
                          subplot_label="(c) Comp/Comm perf (lower is better ↓)",
                          ylabel="Relative Time",
                          ax=axes[2])

    fig.subplots_adjust(left=0.08, right=0.99, top=0.88, bottom=0.18, wspace=0.15)
    out = os.path.join(folder, "eval-microbench")
    plt.savefig(out + ".png", dpi=300, bbox_inches='tight')
    plt.savefig(out + ".pdf", bbox_inches='tight')
    print(f"Saved {out}.png/pdf")
    plt.close()

def plot_ablation_combined(df_dma, folder, font_sizes=None, fig_size=None):
    """Protocol, network, and virtualization DMA overhead plots side-by-side."""
    font_sizes = font_sizes or E2E_COMBINED_FONT_SIZES
    fig_size = fig_size or E2E_COMBINED_FIGURE_SIZE

    fig, axes = plt.subplots(1, 3, figsize=fig_size)

    prot_variants = ['Baseline (BM)', 'Jigsaw-minus-NW (BM)']
    prot_markers = {
        'Baseline (BM)': {'h2d': 'o', 'd2h': 's'},
        'Jigsaw-minus-NW (BM)': {'h2d': '^', 'd2h': 'D'},
    }
    plot_lines(df_dma[df_dma['variant'].isin(prot_variants)],
               "DMA Throughput", "Throughput [GiB/s]",
               None, prot_variants, is_latency=False,
               variant_markers=prot_markers, legend_left=True,
               font_sizes=font_sizes, legend_ncol=2, split_op_legend=True,
               subplot_label="(a) Protocol overheads (higher is better ↑)",
               ax=axes[0])

    net_variants = ['Jigsaw-minus-NW (BM)', 'Jigsaw (BM)']
    net_markers = {
        'Jigsaw-minus-NW (BM)': {'h2d': 'o', 'd2h': 's'},
        'Jigsaw (BM)': {'h2d': '^', 'd2h': 'D'},
    }
    plot_lines(df_dma[df_dma['variant'].isin(net_variants)],
               "DMA Throughput", "Throughput [GiB/s]",
               None, net_variants, is_latency=False,
               variant_markers=net_markers, legend_left=True,
               font_sizes=font_sizes, legend_ncol=2, split_op_legend=True,
               subplot_label="(b) Network overheads (higher is better ↑)",
               ax=axes[1])

    virt_variants = ['Baseline (BM)', 'Baseline', 'Jigsaw (BM)', 'Jigsaw']
    virt_markers = {
        'Baseline (BM)': {'h2d': 'o', 'd2h': 's'},
        'Baseline': {'h2d': '^', 'd2h': 'D'},
        'Jigsaw (BM)': {'h2d': 'v', 'd2h': 'P'},
        'Jigsaw': {'h2d': 'X', 'd2h': 'p'},
    }
    plot_lines(df_dma[df_dma['variant'].isin(virt_variants)],
               "DMA Throughput", "Throughput [GiB/s]",
               None, virt_variants, is_latency=False,
               variant_markers=virt_markers, legend_left=True,
               font_sizes=font_sizes, legend_ncol=2, split_op_legend=True,
               subplot_label="(c) Virtualization overheads (higher is better ↑)",
               ax=axes[2])

    fig.subplots_adjust(left=0.08, right=0.99, top=0.88, bottom=0.18, wspace=0.15)
    out = os.path.join(folder, "ablation-study")
    plt.savefig(out + ".png", dpi=300, bbox_inches='tight')
    plt.savefig(out + ".pdf", bbox_inches='tight')
    print(f"Saved {out}.png/pdf")
    plt.close()

def plot_motivation_combined(stranding_dir, df_motiv_dma, folder,
                             font_sizes=None, fig_size=None):
    """Stranding analysis, DMA perf motivation, and DMA latency motivation side-by-side."""
    font_sizes = font_sizes or E2E_COMBINED_FONT_SIZES
    fig_size = fig_size or E2E_COMBINED_FIGURE_SIZE

    fig, axes = plt.subplots(1, 3, figsize=fig_size)

    motiv_variants = ['Jigsaw', 'CPU-attached']
    motiv_markers = {
        'Jigsaw': {'h2d': 'o', 'd2h': 's'},
        'CPU-attached': {'h2d': '^', 'd2h': 'D'},
    }

    plot_gpu_stranding(stranding_dir, folder,
                       ax=axes[0], font_sizes=font_sizes,
                       subplot_label="(a) GPU stranding")

    plot_lines(df_motiv_dma, "DMA Throughput", "Throughput [GiB/s]",
               None, motiv_variants, is_latency=False,
               variant_markers=motiv_markers, legend_left=True,
               font_sizes=font_sizes,
               subplot_label="(b) DMA perf (higher is better ↑)",
               ax=axes[1])

    df_motiv_lat = df_motiv_dma.copy()
    df_motiv_lat['val'] = df_motiv_lat['latency_ns']
    plot_lines(df_motiv_lat, "DMA Latency", "Latency [us]",
               None, motiv_variants, is_latency=True,
               variant_markers=motiv_markers, legend_left=True,
               font_sizes=font_sizes,
               subplot_label="(c) DMA latency (lower is better ↓)",
               ax=axes[2])

    fig.subplots_adjust(left=0.08, right=0.99, top=0.88, bottom=0.18, wspace=0.15)
    out = os.path.join(folder, "jigsaw-motivation")
    plt.savefig(out + ".png", dpi=300, bbox_inches='tight')
    plt.savefig(out + ".pdf", bbox_inches='tight')
    print(f"Saved {out}.png/pdf")
    plt.close()

def main():
    parser = argparse.ArgumentParser(description='Plot Jigsaw benchmark logs.')
    parser.add_argument('folder', help='Folder containing .log files')
    args = parser.parse_args()

    stranding_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 'stranding_experiments_data')

    # Global Mapping for collection
    all_mapping = {
        'Baseline (BM)': 'no-virt-jigsaw-baseline.log',
        'Baseline': 'jigsaw-baseline.log',
        'Jigsaw-minus-NW (BM)': 'no-virt-jigsaw-minus-nw.log',
        'Jigsaw-minus-NW': 'jigsaw-minus-nw.log',
        'Jigsaw (BM)': 'no-virt-jigsaw-nw-attached.log',
        'Jigsaw': 'jigsaw-nw-attached.log'
    }
    df_mmio, df_dma, df_comp = parse_logs(args.folder, all_mapping)

    # 1. E2E Microbench
    e2e_mapping = {
        'Baseline': 'jigsaw-baseline.log',
        'Jigsaw': 'jigsaw-nw-attached.log'
    }
    df_e2e_mmio, df_e2e_dma, df_e2e_comp = parse_logs(args.folder, e2e_mapping)
    e2e_variants = ['Baseline', 'Jigsaw']
    e2e_markers = {
        'Baseline': {'h2d': 'o', 'd2h': 's', 'write': 'o', 'read': 's'},
        'Jigsaw': {'h2d': '^', 'd2h': 'D', 'write': '^', 'read': 'D'}
    }
    plot_lines(limit_mmio_plot_size(df_e2e_mmio), "MMIO Latency", "Latency [us]",
               os.path.join(args.folder, "MMIO_Performance_e2e_microbench"), e2e_variants, is_latency=True,
               variant_markers=e2e_markers, legend_left=True, direction_y=MMIO_DIRECTION_Y,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE,
               y_max=30, legend_loc='upper center', single_row_legend=True,
               legend_bbox_to_anchor=MMIO_LEGEND_ANCHOR)
    plot_lines(df_e2e_dma, "DMA Throughput", "Throughput [GiB/s]",
               os.path.join(args.folder, "DMA_Performance_e2e_microbench"), e2e_variants, is_latency=False,
               variant_markers=e2e_markers, legend_left=True, direction_y=PERFORMANCE_DIRECTION_Y,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE)
    plot_comp_performance(df_e2e_comp, args.folder, e2e_variants,
                          font_sizes=PERFORMANCE_FONT_SIZES,
                          fig_size=PERFORMANCE_FIGURE_SIZE,
                          direction_y=PERFORMANCE_DIRECTION_Y)
    plot_e2e_combined(df_e2e_mmio, df_e2e_dma, df_e2e_comp, args.folder,
                      e2e_variants, e2e_markers)

    # 2. Protocol Overheads
    prot_mmio_variants = ['Baseline', 'Jigsaw-minus-NW']
    prot_dma_variants = ['Baseline (BM)', 'Jigsaw-minus-NW (BM)']
    prot_mmio_markers = {
        'Baseline': {'h2d': 'o', 'd2h': 's', 'write': 'o', 'read': 's'},
        'Jigsaw-minus-NW': {'h2d': '^', 'd2h': 'D', 'write': '^', 'read': 'D'}
    }
    prot_dma_markers = {
        'Baseline (BM)': {'h2d': 'o', 'd2h': 's'},
        'Jigsaw-minus-NW (BM)': {'h2d': '^', 'd2h': 'D'}
    }
    plot_lines(limit_mmio_plot_size(df_mmio[df_mmio['variant'].isin(prot_mmio_variants)]), "MMIO Latency", "Latency [us]",
               os.path.join(args.folder, "MMIO_Performance_protocol_overheads"), prot_mmio_variants, is_latency=True,
               legend_left=True, direction_y=MMIO_DIRECTION_Y, variant_markers=prot_mmio_markers,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE,
               y_max=30, legend_loc='upper center', single_row_legend=True,
               legend_bbox_to_anchor=MMIO_LEGEND_ANCHOR)
    plot_lines(df_dma[df_dma['variant'].isin(prot_dma_variants)], "DMA Throughput", "Throughput [GiB/s]", 
               os.path.join(args.folder, "DMA_Performance_protocol_overheads"), prot_dma_variants, is_latency=False,
               legend_left=True, direction_y=PERFORMANCE_DIRECTION_Y, variant_markers=prot_dma_markers,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE)

    # 3. Network Overheads
    net_mmio_variants = ['Jigsaw-minus-NW', 'Jigsaw']
    net_dma_variants = ['Jigsaw-minus-NW (BM)', 'Jigsaw (BM)']
    net_mmio_markers = {
        'Jigsaw-minus-NW': {'h2d': 'o', 'd2h': 's', 'write': 'o', 'read': 's'},
        'Jigsaw': {'h2d': '^', 'd2h': 'D', 'write': '^', 'read': 'D'}
    }
    net_dma_markers = {
        'Jigsaw-minus-NW (BM)': {'h2d': 'o', 'd2h': 's'},
        'Jigsaw (BM)': {'h2d': '^', 'd2h': 'D'}
    }
    plot_lines(limit_mmio_plot_size(df_mmio[df_mmio['variant'].isin(net_mmio_variants)]), "MMIO Latency", "Latency [us]",
               os.path.join(args.folder, "MMIO_Performance_network_overheads"), net_mmio_variants, is_latency=True,
               legend_left=True, direction_y=MMIO_DIRECTION_Y, variant_markers=net_mmio_markers,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE,
               y_max=30, legend_loc='upper center', single_row_legend=True,
               legend_bbox_to_anchor=MMIO_LEGEND_ANCHOR)
    plot_lines(df_dma[df_dma['variant'].isin(net_dma_variants)], "DMA Throughput", "Throughput [GiB/s]", 
               os.path.join(args.folder, "DMA_Performance_network_overheads"), net_dma_variants, is_latency=False,
               legend_left=True, direction_y=PERFORMANCE_DIRECTION_Y, variant_markers=net_dma_markers,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE)

    # 4. Virtualization Overheads
    virt_variants = ['Baseline (BM)', 'Baseline', 'Jigsaw (BM)', 'Jigsaw']
    virt_markers = {
        'Baseline (BM)': {'h2d': 'o', 'd2h': 's', 'write': 'o', 'read': 's'},
        'Baseline': {'h2d': '^', 'd2h': 'D', 'write': '^', 'read': 'D'},
        'Jigsaw (BM)': {'h2d': 'v', 'd2h': 'P', 'write': 'v', 'read': 'P'},
        'Jigsaw': {'h2d': 'X', 'd2h': 'p', 'write': 'X', 'read': 'p'}
    }
    plot_lines(limit_mmio_plot_size(df_mmio[df_mmio['variant'].isin(virt_variants)]), "MMIO Latency", "Latency [us]",
               os.path.join(args.folder, "MMIO_Performance_virtualization_overheads"), virt_variants, is_latency=True,
               legend_left=True, direction_y=MMIO_DIRECTION_Y, variant_markers=virt_markers,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE,
               y_max=30, legend_loc='upper center', single_row_legend=True,
               legend_bbox_to_anchor=MMIO_LEGEND_ANCHOR)
    plot_lines(df_dma[df_dma['variant'].isin(virt_variants)], "DMA Throughput", "Throughput [GiB/s]",
               os.path.join(args.folder, "DMA_Performance_virtualization_overheads"), virt_variants, is_latency=False,
               legend_left=True, direction_y=PERFORMANCE_DIRECTION_Y, variant_markers=virt_markers,
               font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE)
    plot_ablation_combined(df_dma, args.folder, font_sizes=ABLATION_COMBINED_FONT_SIZES)

    # 5. Motivation Analysis
    motiv_mapping = {
        'Jigsaw': 'jigsaw-nw-attached.log',
        'CPU-attached': 'cpu-attached-remote.log'
    }
    df_motiv_mmio, df_motiv_dma, _ = parse_logs(args.folder, motiv_mapping)
    motiv_variants = ['Jigsaw', 'CPU-attached']
    motiv_markers = {
        'Jigsaw': {'h2d': 'o', 'd2h': 's'},
        'CPU-attached': {'h2d': '^', 'd2h': 'D'}
    }
    
    if not df_motiv_dma.empty:
        plot_lines(df_motiv_dma, "Motivation Analysis", "Throughput [GiB/s]",
                   os.path.join(args.folder, "DMA_Performance_motivation"), motiv_variants, is_latency=False,
                   legend_left=True, direction_y=MOTIVATION_DIRECTION_Y, variant_markers=motiv_markers,
                   font_sizes=PERFORMANCE_FONT_SIZES, fig_size=PERFORMANCE_FIGURE_SIZE)

        # Motivation Latency Plot
        df_motiv_lat = df_motiv_dma.copy()
        if 'latency_ns' in df_motiv_lat.columns:
            df_motiv_lat['val'] = df_motiv_lat['latency_ns']
            plot_lines(df_motiv_lat, "Motivation Analysis", "Latency [us]",
                       os.path.join(args.folder, "DMA_Latency_motivation"), motiv_variants, is_latency=True,
                       legend_left=True, direction_y=MOTIVATION_DIRECTION_Y, variant_markers=motiv_markers,
                       font_sizes=SINGLE_COLUMN_FONT_SIZES, fig_size=SINGLE_COLUMN_FIGURE_SIZE)

        if os.path.isdir(stranding_dir) and 'latency_ns' in df_motiv_dma.columns:
            plot_motivation_combined(stranding_dir, df_motiv_dma, args.folder,
                                     font_sizes=MOTIVATION_COMBINED_FONT_SIZES)
    else:
        print("No motivation data found.")

    # 6. 4KiB Latency Decomposition (Grouped by Core Variant)
    plot_4k_grouped_latency(df_dma, args.folder)

    # 7. NEW: E2E Application Performance
    app_mapping = {
        'Baseline': 'jigsaw-baseline.log',
        'Jigsaw': 'jigsaw-nw-attached.log',
        'SW-Forwarder': 'jigsaw-sw-forwarder.log'
    }
    df_apps = parse_app_summary(args.folder, app_mapping)
    if not df_apps.empty:
        # One set of app plots per cycles_scale found in the logs; scale 1
        # keeps the legacy unsuffixed filenames, others get -cs<N>. Logs at
        # a non-default scale live in <stem>-cs<N>.log files.
        for scale in sorted(df_apps['cycles_scale'].unique()):
            sub = df_apps[df_apps['cycles_scale'] == scale].drop(columns=['cycles_scale'])
            suffix = '' if scale == 1 else f"-cs{int(scale)}"
            summarize_app_set_changes(sub, args.folder, name_suffix=suffix)
            plot_app_performance(sub, args.folder, name_suffix=suffix)
            plot_compact_app_size_performance(sub, args.folder, name_suffix=suffix)

    # 8. E2E Application Performance (No-Virt Only)
    no_virt_app_mapping = {
        'Baseline (BM)': 'no-virt-jigsaw-baseline.log',
        'Jigsaw-minus-NW (BM)': 'no-virt-jigsaw-minus-nw.log',
        'Jigsaw (BM)': 'no-virt-jigsaw-nw-attached.log'
    }
    df_no_virt_apps = parse_no_virt_app_summary(args.folder, no_virt_app_mapping)
    plot_no_virt_app_performance(df_no_virt_apps, args.folder)

    # 9. GPU Stranding Analysis
    if os.path.isdir(stranding_dir):
        plot_gpu_stranding(stranding_dir, args.folder)
    else:
        print(f"Stranding data dir not found: {stranding_dir}")

if __name__ == "__main__":
    main()
