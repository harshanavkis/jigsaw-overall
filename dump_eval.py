import sys, os, numpy as np, pandas as pd
import matplotlib; matplotlib.use('Agg')
sys.path.insert(0, '/scratch/harshanavkis/jigsaw-ns/jigsaw-overall')
import plot_benchmarks as pb
F='/scratch/harshanavkis/jigsaw-ns/jigsaw-overall/experimental_results'
out=[]
def P(*a): out.append(' '.join(str(x) for x in a))
def md(df, floatfmt='%.3f'):
    cols=list(df.columns); P('| '+' | '.join(cols)+' |'); P('|'+'---|'*len(cols))
    for _,r in df.iterrows(): P('| '+' | '.join((floatfmt % v if isinstance(v,float) else str(v)) for v in r.values)+' |')
    P('')
def sz(s): return f"{s//1048576}M" if s>=1048576 else f"{s//1024}K" if s>=1024 else f"{s}B"
SIZES_ORDER=None

# ---------------- Fig 1(b,c): Jigsaw vs CPU-attached ----------------
dm,dd,_=pb.parse_logs(F,{'Jigsaw':'jigsaw-nw-attached.log','CPU-attached':'cpu-attached-remote.log'})
P('## Figure 1(b,c) — jigsaw-motivation.pdf (plot_motivation_combined)')
P('x = transfer size (4K..1M); (b) y = DMA throughput GiB/s, (c) y = DMA latency us. Value per point = **geometric mean** over iterations. Logs: jigsaw-nw-attached.log (Jigsaw), cpu-attached-remote.log (CPU-attached). Annotation = arithmetic mean at 1 MiB (annotate_improvement).')
g=dd.groupby(['size','op','variant']).agg(thr_gmean=('val',pb.gmean),lat_us_gmean=('latency_ns',lambda x: pb.gmean(x)/1000),thr_mean=('val','mean'),lat_us_mean=('latency_ns',lambda x: x.mean()/1000)).reset_index()
t=g.pivot_table(index=['size','op'],columns='variant',values=['thr_gmean','lat_us_gmean','thr_mean','lat_us_mean']).reset_index()
t.columns=['_'.join(c).strip('_') for c in t.columns]
t['thr_ratio_mean(J/C)']=t['thr_mean_Jigsaw']/t['thr_mean_CPU-attached']; t['lat_reduction_mean%']=100*(1-t['lat_us_mean_Jigsaw']/t['lat_us_mean_CPU-attached'])
t['size']=t['size'].map(sz); md(t)
P('**Text (§1 key idea 3):** 2.7x (H2D) / 2.8x (D2H) throughput, latency -63% / -64% at 1 MiB.'); P('')

# ---------------- Fig 7: microbenchmarks ----------------
mm,dm2,cp=pb.parse_logs(F,{'Baseline':'jigsaw-baseline.log','Jigsaw':'jigsaw-nw-attached.log'})
P('## Figure 7(a) — MMIO_Performance_e2e_microbench.pdf (plot_lines)')
P('x = access size (B), y = latency us, gmean over iterations. Logs: jigsaw-baseline.log, jigsaw-nw-attached.log (MMIO_BENCH_CSV).')
g=mm.groupby(['size','op','variant'])['val'].agg(pb.gmean).reset_index(); t=g.pivot_table(index=['size','op'],columns='variant',values='val').reset_index()
t['ratio(J/B)']=t['Jigsaw']/t['Baseline']; t.columns=['size','op','Baseline_us','Jigsaw_us','ratio(J/B)']; md(t)
r=t.groupby('op')['ratio(J/B)'].agg(['mean','min','max']).reset_index(); P('ratio summary per op (mean/min/max over sizes):'); md(r)
P('**Text:** MMIO read 2.0x, write 1.23x.'); P('')

P('## Figure 7(b) — DMA_Performance_e2e_microbench.pdf (plot_lines)')
P('x = transfer size, y = throughput GiB/s, gmean over iterations. Same logs (DMA_BENCH_CSV).')
g=dm2.groupby(['size','op','variant'])['val'].agg(pb.gmean).reset_index(); t=g.pivot_table(index=['size','op'],columns='variant',values='val').reset_index()
t['J/B %']=100*t['Jigsaw']/t['Baseline']; t['size']=t['size'].map(sz); md(t)
P('**Text:** 78.8% of baseline H2D and 77.3% of baseline D2H at 1 MiB; §1 says "up to 79%".'); P('')

P('## Figure 7(c) — Computation_Performance.pdf (plot_comp_performance)')
P('x = device compute (cycles*4ns), one line per size; y = runtime ratio jigsaw/baseline of gmean(time_ns). COMP_BENCH_CSV.')
s=cp.groupby(['size','variant','cycles'])['time_ns'].agg(pb.gmean).reset_index(); t=s.pivot_table(index=['size','cycles'],columns='variant',values='time_ns').reset_index()
t['ratio']=t['Jigsaw']/t['Baseline']; t['comp_us']=t['cycles']*4/1000; t['size']=t['size'].map(sz); md(t[['size','cycles','comp_us','Baseline','Jigsaw','ratio']])
rc=t.groupby('cycles')['ratio'].agg(['mean','min','max']).reset_index(); rc['comp_us']=rc['cycles']*4/1000; P('ratio per compute level across sizes:'); md(rc)
P('**Text:** 1.68x at 400 ns -> 1.01x at 4 ms.'); P('')

# ---------------- Fig 8: ablation ----------------
allmap={'Baseline (BM)':'no-virt-jigsaw-baseline.log','Baseline':'jigsaw-baseline.log','Jigsaw-minus-NW (BM)':'no-virt-jigsaw-minus-nw.log','Jigsaw-minus-NW':'jigsaw-minus-nw.log','Jigsaw (BM)':'no-virt-jigsaw-nw-attached.log','Jigsaw':'jigsaw-nw-attached.log'}
_,da,_=pb.parse_logs(F,allmap)
g=da.groupby(['size','op','variant'])['val'].agg(pb.gmean).reset_index(); T=g.pivot_table(index=['size','op'],columns='variant',values='val').reset_index()
def panel(name,ref,var,text):
    P(f'## Figure 8{name} — DMA throughput, {var} vs {ref} (plot_ablation_combined -> plot_lines)')
    P(f'x = size, y = throughput GiB/s (gmean over iterations). Logs: {allmap[ref]} vs {allmap[var]}.')
    t=T[['size','op',ref,var]].copy(); t['var/ref %']=100*t[var]/t[ref]; t['reduction %']=100-t['var/ref %']; t['size']=t['size'].map(sz); md(t)
    P(f'mean reduction over all (size,op) points = {t["reduction %"].mean():.1f}%  |  mean over sizes H2D = {t[t.op=="h2d"]["reduction %"].mean():.1f}%, D2H = {t[t.op=="d2h"]["reduction %"].mean():.1f}%  |  1 - gmean(var)/gmean(ref) over all points = {100*(1-pb.gmean(t[var])/pb.gmean(t[ref])):.1f}%')
    P(f'**Text:** {text}'); P('')
panel('(a) protocol','Baseline (BM)','Jigsaw-minus-NW (BM)','32.8% reduction on average; 95.9% (H2D) / 94.4% (D2H) of bare-metal at 1 MiB.')
panel('(b) network','Jigsaw-minus-NW (BM)','Jigsaw (BM)','52.2% reduction on average; 83.2% (H2D) / 81.1% (D2H) of no-network at 1 MiB.')
panel('(c) virtualization, baseline','Baseline (BM)','Baseline','VM-local baseline reduces throughput by 63.4% relative to bare-metal.')
panel('(c) virtualization, jigsaw','Jigsaw (BM)','Jigsaw','jigsaw reduces throughput by 28.5% relative to jigsaw (Native).')

P('## Figure 8(d) — DMA_Latency_Decomposition_4K.pdf (plot_4k_grouped_latency)')
P('4 KiB H2D only. init = gmean(mmio_done_ns), exec = gmean(latency_ns) - gmean(mmio_done_ns); bars per variant (BM and VM).')
s4=da[(da['size']==4096)&(da['op']=='h2d')].groupby('variant').agg(lat_us=('latency_ns',lambda x: pb.gmean(x)/1000),init_us=('mmio_done_ns',lambda x: pb.gmean(x)/1000),n=('val','size')).reset_index()
s4['exec_us']=s4['lat_us']-s4['init_us']; s4['init %']=100*s4['init_us']/s4['lat_us']; md(s4)
P('**Text:** Jigsaw: init 45.6% = 18.5 us of 40.6 us; VM-local baseline 62.4%; no-network Jigsaw 55.8%.'); P('')

# ---------------- Fig 6: E2E apps ----------------
apps=pb.parse_app_summary(F,{'Baseline':'jigsaw-baseline.log','Jigsaw':'jigsaw-nw-attached.log','SW-Forwarder':'jigsaw-sw-forwarder.log'})
P('## Figure 6 — E2E_App_Performance-long-cs6.pdf (plot_app_performance)')
P('x = application (19, paper order), y = total trace time (us) per variant, bars = gmean over runs of TRACE_SUMMARY total_ns; annotation = ratio to Baseline. Logs: *-cs6.log (cycles_scale=6), set = long.')
for scale in sorted(apps['cycles_scale'].unique()):
    for st in ['long','medium','small']:
        sub=apps[(apps['cycles_scale']==scale)&(apps['set']==st)]
        if sub.empty: continue
        t=sub.pivot_table(index='app',columns='variant',values='total_us').reset_index()
        t['Jigsaw/Baseline']=t['Jigsaw']/t['Baseline']
        if 'SW-Forwarder' in t: t['SWF/Baseline']=t['SW-Forwarder']/t['Baseline']
        P(f'### cycles_scale={int(scale)}, set={st}'); md(t)
        gj=pb.gmean(t['Jigsaw/Baseline']); P(f'geomean Jigsaw/Baseline = {gj:.4f} ({100*(gj-1):.1f}%), worst = {t.loc[t["Jigsaw/Baseline"].idxmax(),"app"]} {t["Jigsaw/Baseline"].max():.3f}')
        if 'SWF/Baseline' in t:
            gs=pb.gmean(t['SWF/Baseline']); P(f'geomean SWF/Baseline = {gs:.4f} ({100*(gs-1):.1f}%), worst = {t.loc[t["SWF/Baseline"].idxmax(),"app"]} {t["SWF/Baseline"].max():.3f}; overhead ratio SWF/Jigsaw = {(gs-1)/(gj-1):.2f}x')
        P('')
P('**Text (§6.1):** geomean slowdown 9.9%, worst bfs 46%; vfio-fwd 24.5% geomean, up to 87% (bfs), ~2.5x Jigsaw overhead; compute bundles = 91.8% of baseline and 87.5% of Jigsaw runtime.')
# compute share from raw TRACE_SUMMARY
import re
def bundle_share(fn):
    tot=bund=0
    for line in open(os.path.join(F,fn)):
        m=re.search(r'TRACE_SUMMARY:\s*(long),([^,]+),(\d+),(\d+),(\d+),(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)',line)
        if m and m.group(2)!='oclprintf': bund+=int(m.group(10)); tot+=int(m.group(11))
    return 100*bund/tot if tot else float('nan')
P(f'compute-bundle share of total (set=long, summed over apps and runs): baseline {bundle_share("jigsaw-baseline-cs6.log"):.1f}%, jigsaw {bundle_share("jigsaw-nw-attached-cs6.log"):.1f}%'); P('')
open('/scratch/harshanavkis/jigsaw-ns/jigsaw-hpca-paper/EVAL_DATA_CHECK.md','w').write('\n'.join(out)); print("written", len(out), "lines")
